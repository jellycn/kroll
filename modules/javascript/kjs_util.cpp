/**
 * Appcelerator Kroll - licensed under the Apache Public License 2
 * see LICENSE in the root folder for details on the license.
 * Copyright (c) 2008 Appcelerator, Inc. All Rights Reserved.
 */

#include "javascript_module.h"
#include <Poco/FileStream.h>

namespace kroll
{
	JSClassRef tibo_class = NULL;
	JSClassRef tibm_class = NULL;
	JSClassRef tibl_class = NULL;
	const JSClassDefinition empty_class = { 0, 0, 0, 0, 0, 0,
	                                        0, 0, 0, 0, 0, 0,
	                                        0, 0, 0, 0, 0 };

	/* callback for KObject proxying to KJS */
	void get_property_names_cb(JSContextRef, JSObjectRef, JSPropertyNameAccumulatorRef);
	bool has_property_cb(JSContextRef, JSObjectRef, JSStringRef);
	JSValueRef get_property_cb(JSContextRef, JSObjectRef, JSStringRef, JSValueRef*);
	bool set_property_cb(JSContextRef, JSObjectRef, JSStringRef, JSValueRef, JSValueRef*);
	JSValueRef call_as_function_cb(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
	void finalize_cb(JSObjectRef);

	void add_special_property_names(SharedValue, SharedStringList, bool);
	JSValueRef get_special_property(SharedValue, char*, JSContextRef, SharedValue);
	JSValueRef to_string_cb(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
	JSValueRef equals_cb(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);

	SharedValue KJSUtil::ToKrollValue(
		JSValueRef value,
		JSContextRef ctx,
		JSObjectRef this_obj)
	{
		SharedValue kr_val;
		JSValueRef exception = NULL;

		if (value == NULL)
		{
			std::cerr << "Trying to convert NULL JSValueRef!" << std::endl;
			return Value::Undefined;
		}

		if (JSValueIsNumber(ctx, value))
		{
			kr_val = Value::NewDouble(JSValueToNumber(ctx, value, &exception));
		}
		else if (JSValueIsBoolean(ctx, value))
		{
			kr_val = Value::NewBool(JSValueToBoolean(ctx, value));
		}
		else if (JSValueIsString(ctx, value))
		{

			JSStringRef string_ref = JSValueToStringCopy(ctx, value, &exception);
			if (string_ref)
			{
				char* chars = KJSUtil::ToChars(string_ref);
				std::string to_ret = std::string(chars);
				JSStringRelease(string_ref);
				free(chars);
				kr_val = Value::NewString(to_ret);
			}

		}
		else if (JSValueIsObject(ctx, value))
		{
			JSObjectRef o = JSValueToObject(ctx, value, &exception);
			if (o != NULL)
			{
				SharedValue* value = static_cast<SharedValue*>(JSObjectGetPrivate(o));
				if (value != NULL)
				{
					// This is a KJS-wrapped Kroll value: unwrap it
					return *value;
				}
				else if (JSObjectIsFunction(ctx, o))
				{
					// this is a pure JS method: proxy it
					SharedKMethod tibm = new KKJSMethod(ctx, o, this_obj);
					kr_val = Value::NewMethod(tibm);
				}
				else if (IsArrayLike(o, ctx))
				{
					// this is a pure JS array: proxy it
					SharedKList tibl = new KKJSList(ctx, o);
					kr_val = Value::NewList(tibl);
				}
				else
				{
					// this is a pure JS object: proxy it
					SharedKObject tibo = new KKJSObject(ctx, o);
					kr_val = Value::NewObject(tibo);
				}
			}
		}
		else if (JSValueIsNull(ctx, value))
		{
			kr_val = kroll::Value::Null;
		}
		else
		{
			kr_val = kroll::Value::Undefined;
		}

		if (!kr_val.isNull() && exception == NULL)
		{
			return kr_val;
		}
		else
		{
			throw KJSUtil::ToKrollValue(exception, ctx, NULL);
		}
	}

	JSValueRef KJSUtil::ToJSValue(SharedValue value, JSContextRef ctx)
	{
		JSValueRef js_val;
		if (value->IsInt())
		{
			js_val = JSValueMakeNumber(ctx, value->ToInt());
		}
		else if (value->IsDouble())
		{
			js_val = JSValueMakeNumber(ctx, value->ToDouble());
		}
		else if (value->IsBool())
		{
			js_val = JSValueMakeBoolean(ctx, value->ToBool());
		}
		else if (value->IsString())
		{
			JSStringRef s = JSStringCreateWithUTF8CString(value->ToString());
			js_val = JSValueMakeString(ctx, s);
			JSStringRelease(s);
		}
		else if (value->IsObject())
		{
			SharedKObject obj = value->ToObject();
			SharedPtr<KKJSObject> kobj = obj.cast<KKJSObject>();
			if (!kobj.isNull() && kobj->SameContextGroup(ctx))
			{
				// this object is actually a pure JS object
				js_val = kobj->GetJSObject();
			}
			else
			{
				// this is a KObject that needs to be proxied
				js_val = KJSUtil::KObjectToJSValue(value, ctx);
			}
		}
		else if (value->IsMethod())
		{
			SharedKMethod meth = value->ToMethod();
			SharedPtr<KKJSMethod> kmeth = meth.cast<KKJSMethod>();
			if (!kmeth.isNull() && kmeth->SameContextGroup(ctx))
			{
				// this object is actually a pure JS callable object
				js_val = kmeth->GetJSObject();
			}
			else
			{
				// this is a KMethod that needs to be proxied
				js_val = KJSUtil::KMethodToJSValue(value, ctx);
			}
		}
		else if (value->IsList())
		{
			SharedKList list = value->ToList();
			SharedPtr<KKJSList> klist = list.cast<KKJSList>();
			if (!klist.isNull() && klist->SameContextGroup(ctx))
			{
				// this object is actually a pure JS array
				js_val = klist->GetJSObject();
			}
			else
			{
				// this is a KList that needs to be proxied
				js_val = KJSUtil::KListToJSValue(value, ctx);
			}
		}
		else if (value->IsNull())
		{
			js_val = JSValueMakeNull(ctx);
		}
		else if (value->IsUndefined())
		{
			js_val = JSValueMakeUndefined(ctx);
		}
		else
		{
			js_val = JSValueMakeUndefined(ctx);
		}

		return js_val;

	}

	JSValueRef KJSUtil::KObjectToJSValue(SharedValue obj_val, JSContextRef c)
	{
		if (tibo_class == NULL)
		{
			JSClassDefinition js_class_def = empty_class;
			js_class_def.className = "Object";
			js_class_def.getPropertyNames = get_property_names_cb;
			js_class_def.finalize = finalize_cb;
			js_class_def.hasProperty = has_property_cb;
			js_class_def.getProperty = get_property_cb;
			js_class_def.setProperty = set_property_cb;
			tibo_class = JSClassCreate(&js_class_def);
		}
		return JSObjectMake(c, tibo_class, new SharedValue(obj_val));
	}

	JSValueRef KJSUtil::KMethodToJSValue(SharedValue meth_val, JSContextRef c)
	{
		if (tibm_class == NULL)
		{
			JSClassDefinition js_class_def = empty_class;
			js_class_def.className = "Function";
			js_class_def.getPropertyNames = get_property_names_cb;
			js_class_def.finalize = finalize_cb;
			js_class_def.hasProperty = has_property_cb;
			js_class_def.getProperty = get_property_cb;
			js_class_def.setProperty = set_property_cb;
			js_class_def.callAsFunction = call_as_function_cb;
			tibm_class = JSClassCreate(&js_class_def);
		}
		JSObjectRef ref = JSObjectMake(c, tibm_class, new SharedValue(meth_val));
		JSValueRef fnProtoValue = GetFunctionPrototype(c, NULL);
		JSObjectSetPrototype(c, ref, fnProtoValue);
		return ref;
	}

	void inline CopyJSProperty(
		JSContextRef c,
		JSObjectRef from_obj,
		SharedKObject to_bo,
		JSObjectRef to_obj,
		const char *prop_name)
	{

		JSStringRef prop_name_str = JSStringCreateWithUTF8CString(prop_name);
		JSValueRef prop = JSObjectGetProperty(c, from_obj, prop_name_str, NULL);
		JSStringRelease(prop_name_str);
		SharedValue prop_val = KJSUtil::ToKrollValue(prop, c, to_obj);
		to_bo->Set(prop_name, prop_val);
	}

	JSValueRef KJSUtil::KListToJSValue(SharedValue list_val, JSContextRef c)
	{

		if (tibl_class == NULL)
		{
			JSClassDefinition js_class_def = empty_class;
			js_class_def.className = "Array";
			js_class_def.getPropertyNames = get_property_names_cb;
			js_class_def.finalize = finalize_cb;
			js_class_def.hasProperty = has_property_cb;
			js_class_def.getProperty = get_property_cb;
			js_class_def.setProperty = set_property_cb;
			tibl_class = JSClassCreate(&js_class_def);
		}

		JSObjectRef ref = JSObjectMake(c, tibl_class, new SharedValue(list_val));
		JSValueRef aProtoValue = GetArrayPrototype(c, NULL);
		JSObjectSetPrototype(c, ref, aProtoValue);
		return ref;
	}

	char* KJSUtil::ToChars(JSStringRef js_string)
	{
		size_t size = JSStringGetMaximumUTF8CStringSize(js_string);
		char* string = (char*) malloc(size);
		JSStringGetUTF8CString(js_string, string, size);
		return string;
	}

	bool KJSUtil::IsArrayLike(JSObjectRef object, JSContextRef c)
	{
		bool array_like = true;

		JSStringRef pop = JSStringCreateWithUTF8CString("pop");
		array_like = array_like && JSObjectHasProperty(c, object, pop);
		JSStringRelease(pop);

		JSStringRef concat = JSStringCreateWithUTF8CString("concat");
		array_like = array_like && JSObjectHasProperty(c, object, concat);
		JSStringRelease(concat);

		JSStringRef length = JSStringCreateWithUTF8CString("length");
		array_like = array_like && JSObjectHasProperty(c, object, length);
		JSStringRelease(length);

		return array_like;
	}

	void finalize_cb(JSObjectRef js_object)
	{
		SharedValue* value = static_cast<SharedValue*>(JSObjectGetPrivate(js_object));
		delete value;
	}

	bool has_property_cb(
		JSContextRef js_context,
		JSObjectRef js_object,
		JSStringRef js_property)
	{
		SharedValue* value = static_cast<SharedValue*>(JSObjectGetPrivate(js_object));
		if (value == NULL)
			return false;

		SharedKObject object = (*value)->ToObject();
		char *name = KJSUtil::ToChars(js_property);
		std::string str_name(name);
		free(name);

		SharedStringList names = object->GetPropertyNames();
		add_special_property_names(*value, names, true);
		for (size_t i = 0; i < names->size(); i++)
		{
			if (str_name == *names->at(i))
			{
				return true;
			}
		}

		return false;
	}

	JSValueRef get_property_cb(
		JSContextRef js_context,
		JSObjectRef js_object,
		JSStringRef js_property,
		JSValueRef* js_exception)
	{

		SharedValue* value = static_cast<SharedValue*>(JSObjectGetPrivate(js_object));
		if (value == NULL)
			return JSValueMakeUndefined(js_context);

		SharedKObject object = (*value)->ToObject();
		char* name = KJSUtil::ToChars(js_property);
		JSValueRef js_val = NULL;
		try
		{
			SharedValue ti_val = object->Get(name);
			js_val = get_special_property(*value, name, js_context, ti_val);
		}
		catch (ValueException& exception)
		{
			*js_exception = KJSUtil::ToJSValue(exception.GetValue(), js_context);
		}
		catch (std::exception &e)
		{
			SharedValue v = Value::NewString(e.what());
			*js_exception = KJSUtil::ToJSValue(v, js_context);
		}
		catch (...)
		{
			std::cerr << "KJSUtil.cpp: Caught an unknown exception during get for "
			          << name << std::endl;
			SharedValue v = Value::NewString("unknown exception");
			*js_exception = KJSUtil::ToJSValue(v, js_context);
		}

		free(name);
		return js_val;
	}

	bool set_property_cb(
		JSContextRef js_context,
		JSObjectRef js_object,
		JSStringRef js_property,
		JSValueRef js_value,
		JSValueRef* js_exception)
	{
		SharedValue* value = static_cast<SharedValue*>(JSObjectGetPrivate(js_object));
		if (value == NULL)
			return false;

		SharedKObject object = (*value)->ToObject();
		bool success = false;
		char* prop_name = KJSUtil::ToChars(js_property);
		try
		{
			SharedValue ti_val = KJSUtil::ToKrollValue(js_value, js_context, js_object);
			object->Set(prop_name, ti_val);
			success = true;
		}
		catch (ValueException& exception)
		{
			*js_exception = KJSUtil::ToJSValue(exception.GetValue(), js_context);
		}
		catch (std::exception &e)
		{
			SharedValue v = Value::NewString(e.what());
			*js_exception = KJSUtil::ToJSValue(v, js_context);
		}
		catch (...)
		{
			std::cerr << "KJSUtil.cpp: Caught an unknown exception during set for "
			          << prop_name << std::endl;
			SharedValue v = Value::NewString("unknown exception");
			*js_exception = KJSUtil::ToJSValue(v, js_context);
		}

		free(prop_name);
		return success;
	}

	JSValueRef call_as_function_cb(
		JSContextRef js_context,
		JSObjectRef js_function,
		JSObjectRef js_this,
		size_t num_args,
		const JSValueRef js_args[],
		JSValueRef* js_exception)
	{
		SharedValue* value = static_cast<SharedValue*>(JSObjectGetPrivate(js_function));
		if (value == NULL)
			return JSValueMakeUndefined(js_context);

		SharedKMethod method = (*value)->ToMethod();
		ValueList args;
		for (size_t i = 0; i < num_args; i++) {
			SharedValue arg_val = KJSUtil::ToKrollValue(js_args[i], js_context, js_this);
			args.push_back(arg_val);
		}

		JSValueRef js_val = NULL;
		try
		{
			SharedValue ti_val = method->Call(args);
			js_val = KJSUtil::ToJSValue(ti_val, js_context);
		}
		catch (ValueException& exception)
		{
			*js_exception = KJSUtil::ToJSValue(exception.GetValue(), js_context);
		}
		catch (std::exception &e)
		{
			SharedValue v = Value::NewString(e.what());
			*js_exception = KJSUtil::ToJSValue(v, js_context);
		}
		catch (...)
		{
			std::cerr << "KJSUtil.cpp: Caught an unknown exception during call()"
			          << std::endl;
			SharedValue v = Value::NewString("unknown exception");
			*js_exception = KJSUtil::ToJSValue(v, js_context);
		}

		return js_val;
	}

	void add_special_property_names(SharedValue value, SharedStringList props, bool showInvisible)
	{
		// Some attributes should be hidden unless the are requested specifically -- 
		// essentially a has_property(...) versus  get_property_list(...). An example
		// of this type of attribute is toString(). Some JavaScript code might expect
		// a "hash" object to have no methods in its property list. We don't want
		// toString() to show up in those situations.

		bool foundLength = false, foundToString = false, foundEquals = false;
		for (size_t i = 0; i < props->size(); i++)
		{
			SharedString pn = props->at(i);
			if (strcmp(pn->c_str(), "length") == 0)
				foundLength = true;
			if (strcmp(pn->c_str(), "toString") == 0)
				foundToString = true;
			if (strcmp(pn->c_str(), "toString") == 0)
				foundToString = true;
		}

		if (!foundLength && value->IsList())
		{
			props->push_back(new std::string("length"));
		}

		if (!foundToString && showInvisible)
		{
			props->push_back(new std::string("toString"));
		}

		if (!foundEquals && showInvisible)
		{
			props->push_back(new std::string("equals"));
		}
	}

	JSValueRef get_special_property(SharedValue value, char* name, JSContextRef ctx, SharedValue objValue)
	{
		// Always override the length property on lists. Some languages
		// supply their own length property, which might be a method instead
		// of a number -- bad news.
		if (value->IsList() && !strcmp(name, "length"))
		{
			SharedKList l = value->ToList();
			return JSValueMakeNumber(ctx, l->Size());
		}

		// Only overload these methods if the value in our object is not a
		// method We want the user to be able to supply their own versions,
		// but we don't want JavaScript code to freak out in situations where
		// Kroll objects use attributes with the same name that aren't methods.
		if (!objValue->IsMethod())
		{
			if (!strcmp(name, "toString"))
			{
				JSStringRef s = JSStringCreateWithUTF8CString("toString");
				return JSObjectMakeFunctionWithCallback(ctx, s, &to_string_cb);
			}

			if (!strcmp(name, "equals"))
			{
				JSStringRef s = JSStringCreateWithUTF8CString("equals");
				return JSObjectMakeFunctionWithCallback(ctx, s, &equals_cb);
			}
		}

		// Otherwise this is just a normal JS value
		return KJSUtil::ToJSValue(objValue, ctx);
	}

	JSValueRef to_string_cb(
		JSContextRef js_context,
		JSObjectRef js_function,
		JSObjectRef js_this,
		size_t num_args,
		const JSValueRef args[],
		JSValueRef* exception)
	{
		SharedValue* value = static_cast<SharedValue*>(JSObjectGetPrivate(js_this));
		if (value == NULL)
			return JSValueMakeUndefined(js_context);

		SharedString ss = (*value)->DisplayString(2);
		SharedValue dsv = Value::NewString(ss);
		return KJSUtil::ToJSValue(dsv, js_context);
	}

	JSValueRef equals_cb(
		JSContextRef ctx,
		JSObjectRef function,
		JSObjectRef jsThis,
		size_t numArgs,
		const JSValueRef args[],
		JSValueRef* exception)
	{
		SharedValue* value = static_cast<SharedValue*>(JSObjectGetPrivate(jsThis));
		if (value == NULL || numArgs < 1)
		{
			return JSValueMakeBoolean(ctx, false);
		}

		// Ensure argument is a JavaScript object
		if (!JSValueIsObject(ctx, args[0]))
		{
			return JSValueMakeBoolean(ctx, false);
		}

		// Ensure argument is a Kroll JavaScript
		JSObjectRef otherObject = JSValueToObject(ctx, args[0], NULL);
		SharedValue* otherValue = static_cast<SharedValue*>(JSObjectGetPrivate(otherObject));
		if (otherValue == NULL)
		{
			return JSValueMakeBoolean(ctx, false);
		}

		// Test equality
		return JSValueMakeBoolean(ctx, (*value)->Equals(*otherValue));
	}


	void get_property_names_cb(
		JSContextRef js_context,
		JSObjectRef js_object,
		JSPropertyNameAccumulatorRef js_properties)
	{
		SharedValue* value = static_cast<SharedValue*>(JSObjectGetPrivate(js_object));
		if (value == NULL)
			return;

		SharedKObject object = (*value)->ToObject();
		SharedStringList props = object->GetPropertyNames();
		add_special_property_names(*value, props, false);
		for (size_t i = 0; i < props->size(); i++)
		{
			SharedString pn = props->at(i);
			JSStringRef name = JSStringCreateWithUTF8CString(pn->c_str());
			JSPropertyNameAccumulatorAddName(js_properties, name);
			JSStringRelease(name);
		}
	}

	std::map<JSObjectRef, JSGlobalContextRef> KJSUtil::contextMap;
	void KJSUtil::RegisterGlobalContext(
		JSObjectRef object,
		JSGlobalContextRef globalContext)
	{
		contextMap[object] = globalContext;
	}

	JSGlobalContextRef KJSUtil::GetGlobalContext(JSObjectRef object)
	{
		if (contextMap.find(object) == contextMap.end())
		{
			return NULL;
		}
		else
		{
			return contextMap[object];
		}
	}

	std::map<JSGlobalContextRef, int> KJSUtil::contextRefCounts;
	void KJSUtil::ProtectGlobalContext(JSGlobalContextRef globalContext)
	{
		if (contextRefCounts.find(globalContext) == contextRefCounts.end())
		{
			JSGlobalContextRetain(globalContext);
			contextRefCounts[globalContext] = 1;
		}
		else
		{
			contextRefCounts[globalContext]++;
		}
	}

	void KJSUtil::UnprotectGlobalContext(JSGlobalContextRef globalContext)
	{
		std::map<JSGlobalContextRef, int>::iterator i
			= contextRefCounts.find(globalContext);

		if (i == contextRefCounts.end())
		{
			std::cerr << 
				"Tried to unprotect an unprotected global context!" << std::endl;
		}
		else if (i->second == 1)
		{
			JSGlobalContextRelease(globalContext);
			contextRefCounts.erase(i);
		}
		else
		{
			contextRefCounts[globalContext]--;
		}
	}

	SharedValue KJSUtil::Evaluate(JSContextRef context, char *script)
	{
		JSObjectRef global_object = JSContextGetGlobalObject(context);
		JSStringRef script_contents = JSStringCreateWithUTF8CString(script);
		JSStringRef url = JSStringCreateWithUTF8CString("<string>");
		JSValueRef exception = NULL;

		JSValueRef return_value = JSEvaluateScript(context, script_contents, global_object, url, 0, &exception);

		JSStringRelease(url);
		JSStringRelease(script_contents);

		if (exception != NULL) {
			throw KJSUtil::ToKrollValue(exception, context, NULL);
		}

		return ToKrollValue(return_value, context, global_object);
	}

	SharedValue KJSUtil::EvaluateFile(JSContextRef context, char *full_path)
	{
		PRINTD("Evaluating JS: " << full_path);
		JSObjectRef global_object = JSContextGetGlobalObject(context);
		Poco::FileInputStream* script_stream = new Poco::FileInputStream(full_path, std::ios::in);
		std::string script_contents;
		while (!script_stream->eof())
		{
			std::string s;
			std::getline(*script_stream, s);

			script_contents.append(s);
			script_contents.append("\n");
		}
		script_stream->close();
		
		JSStringRef script = JSStringCreateWithUTF8CString(script_contents.c_str());
		JSStringRef full_path_url = JSStringCreateWithUTF8CString(full_path);
		JSValueRef exception = NULL;
		JSValueRef return_value = JSEvaluateScript(context, script, global_object, full_path_url, 0, &exception);
		JSStringRelease(script);
		JSStringRelease(full_path_url);
		delete script_stream;

		if (exception != NULL) {
			SharedValue v = KJSUtil::ToKrollValue(exception, context, NULL);
			throw ValueException(v);
		}

		return KJSUtil::ToKrollValue(return_value, context, global_object);
	}

	SharedValue KJSUtil::EvaluateInNewContext(Host *host, SharedKObject properties, SharedKObject global_properties, const char *script)
	{
		JSValueRef exception;
		JSGlobalContextRef context = JSGlobalContextCreate(NULL);
		JSObjectRef global_object = JSContextGetGlobalObject(context);
		KJSUtil::RegisterGlobalContext(global_object, context);
		
		/* Take some steps to insert the API into the Javascript context */
		/* Create a crazy, crunktown delegate hybrid object for Javascript */
		SharedValue global_value = Value::NewObject(host->GetGlobalObject());
		
		// make a void ptr to the JSContext
		SharedKObject go = global_value->ToObject();
		go->Set("$js_context",Value::NewVoidPtr(context));
					
		
		/* convert JS API to a KJS object */
		JSValueRef js_api = KJSUtil::ToJSValue(global_value, context);
		
		/* set the API as a property of the global object */
		JSStringRef prop_name = JSStringCreateWithUTF8CString(PRODUCT_NAME);
		JSObjectSetProperty(context, global_object, prop_name,
		                    js_api, kJSPropertyAttributeNone, NULL);
		JSStringRelease(prop_name);
		
		/* set the global object top-level properties */
		if (!properties.isNull())
		{
			properties->Set("$js_context",Value::NewVoidPtr(context));
			
			SharedKObject globalObj = global_value->ToObject();
			SharedStringList list = properties->GetPropertyNames();
			for (size_t i = 0; i < list->size(); i++)
			{
				SharedString name = list->at(i);
				const char *n = (*name).c_str();
				SharedValue value =	properties->Get(n);
				globalObj->Set(n,value);
			}
		}
		
		Logger *logger = Logger::Get("KJSUtil");
		
		/* set any global properties */
		if (!global_properties.isNull())
		{
			global_properties->Set("$js_context",Value::NewVoidPtr(context));
			
			SharedStringList list = global_properties->GetPropertyNames();
			for (size_t i = 0; i < list->size(); i++)
			{
				SharedString name = list->at(i);
				const char *n = (*name).c_str();

				logger->Debug(">>>> binding %s", n);
				
				SharedValue value =	global_properties->Get(n);
				JSStringRef prop_name = JSStringCreateWithUTF8CString(n);
				JSValueRef value_ref = KJSUtil::ToJSValue(value, context);
				JSObjectSetProperty(context, global_object, prop_name,
				                    value_ref, kJSPropertyAttributeNone, NULL);
				JSStringRelease(prop_name);
			}
		}
		
		/* Try to run the script */
		JSStringRef js_code = JSStringCreateWithUTF8CString(script);
		
		/* check script syntax */
		bool syntax = JSCheckScriptSyntax(context, js_code, NULL, 0, &exception);
		if (!syntax)
		{
			SharedValue e = KJSUtil::ToKrollValue(exception, context, NULL);
			throw ValueException(e);
		}
		
		/* evaluate the script */
		JSValueRef ret = JSEvaluateScript(context, js_code,
		                                  NULL, NULL,
		                                  1, &exception);
		JSStringRelease(js_code);
		
		if (ret == NULL)
		{
			SharedValue e = KJSUtil::ToKrollValue(exception, context, NULL);
			throw ValueException(e);
		}
		
		return ToKrollValue(js_api, context, global_object);
	}

	//===========================================================================//
	// METHODS BORROWED ARE TAKEN FROM GWT - modifications under same license
	//===========================================================================//
	/*
	 * Copyright 2008 Google Inc.
	 * 
	 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
	 * use this file except in compliance with the License. You may obtain a copy of
	 * the License at
	 * 
	 * http://www.apache.org/licenses/LICENSE-2.0
	 * 
	 * Unless required by applicable law or agreed to in writing, software
	 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
	 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
	 * License for the specific language governing permissions and limitations under
	 * the License.
	 */
	
	/*
	 * The following takes the prototype from the Function constructor, this allows
	 * us to easily support call and apply on our objects that support CallAsFunction.
	 *
	 * NOTE: The return value is not protected.
	 */
	JSValueRef KJSUtil::GetFunctionPrototype(JSContextRef jsContext, JSValueRef* exception) 
	{
		JSObjectRef globalObject = JSContextGetGlobalObject(jsContext);
		JSStringRef fnPropName = JSStringCreateWithUTF8CString("Function");
		JSValueRef fnCtorValue = JSObjectGetProperty(jsContext, globalObject,
			fnPropName, exception);
		JSStringRelease(fnPropName);
		if (!fnCtorValue)
		{
			return JSValueMakeUndefined(jsContext);
		}

		JSObjectRef fnCtorObject = JSValueToObject(jsContext, fnCtorValue, exception);
		if (!fnCtorObject)
		{
			return JSValueMakeUndefined(jsContext);
		}

		JSStringRef protoPropName = JSStringCreateWithUTF8CString("prototype");
		JSValueRef fnPrototype = JSObjectGetProperty(jsContext, fnCtorObject,
			protoPropName, exception);
		JSStringRelease(protoPropName);
		if (!fnPrototype)
		{
			return JSValueMakeUndefined(jsContext);
		}

	return fnPrototype;
	}

	/*
	 * The following takes the prototype from the Array constructor, this allows
	 * us to easily support array like functions
	 *
	 * NOTE: The return value is not protected.
	 */
	JSValueRef KJSUtil::GetArrayPrototype(JSContextRef jsContext, JSValueRef* exception) 
	{
		JSObjectRef globalObject = JSContextGetGlobalObject(jsContext);
		JSStringRef fnPropName = JSStringCreateWithUTF8CString("Array");
		JSValueRef fnCtorValue = JSObjectGetProperty(jsContext, globalObject,
			fnPropName, exception);
		JSStringRelease(fnPropName);
		if (!fnCtorValue) 
		{
			return JSValueMakeUndefined(jsContext);
		}

		JSObjectRef fnCtorObject = JSValueToObject(jsContext, fnCtorValue, exception);
		if (!fnCtorObject)
		{
			return JSValueMakeUndefined(jsContext);
		}

		JSStringRef protoPropName = JSStringCreateWithUTF8CString("prototype");
		JSValueRef fnPrototype = JSObjectGetProperty(jsContext, fnCtorObject,
			protoPropName, exception);
		JSStringRelease(protoPropName);
		if (!fnPrototype) 
		{
			return JSValueMakeUndefined(jsContext);
		}

		return fnPrototype;
	}

}
