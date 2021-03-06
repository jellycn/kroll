// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// process_state.h: A snapshot of a process, in a fully-digested state.
//
// Author: Mark Mentovai

#ifndef GOOGLE_BREAKPAD_PROCESSOR_PROCESS_STATE_H__
#define GOOGLE_BREAKPAD_PROCESSOR_PROCESS_STATE_H__

#include <string>
#include <vector>
#include "google_breakpad/processor/system_info.h"
#include "google_breakpad/common/breakpad_types.h"

namespace google_breakpad {

using std::string;
using std::vector;

class CallStack;
class CodeModules;

class ProcessState {
 public:
  ProcessState() : modules_(NULL) { Clear(); }
  ~ProcessState();

  // Resets the ProcessState to its default values
  void Clear();

  // Accessors.  See the data declarations below.
  u_int32_t time_date_stamp() const { return time_date_stamp_; }
  bool crashed() const { return crashed_; }
  string crash_reason() const { return crash_reason_; }
  u_int64_t crash_address() const { return crash_address_; }
  int requesting_thread() const { return requesting_thread_; }
  const vector<CallStack*>* threads() const { return &threads_; }
  const SystemInfo* system_info() const { return &system_info_; }
  const CodeModules* modules() const { return modules_; }

 private:
  // MinidumpProcessor is responsible for building ProcessState objects.
  friend class MinidumpProcessor;

  // The time-date stamp of the minidump (time_t format)
  u_int32_t time_date_stamp_;

  // True if the process crashed, false if the dump was produced outside
  // of an exception handler.
  bool crashed_;

  // If the process crashed, the type of crash.  OS- and possibly CPU-
  // specific.  For example, "EXCEPTION_ACCESS_VIOLATION" (Windows),
  // "EXC_BAD_ACCESS / KERN_INVALID_ADDRESS" (Mac OS X), "SIGSEGV"
  // (other Unix).
  string crash_reason_;

  // If the process crashed, and if crash_reason implicates memory,
  // the memory address that caused the crash.  For data access errors,
  // this will be the data address that caused the fault.  For code errors,
  // this will be the address of the instruction that caused the fault.
  u_int64_t crash_address_;

  // The index of the thread that requested a dump be written in the
  // threads vector.  If a dump was produced as a result of a crash, this
  // will point to the thread that crashed.  If the dump was produced as
  // by user code without crashing, and the dump contains extended Breakpad
  // information, this will point to the thread that requested the dump.
  // If the dump was not produced as a result of an exception and no
  // extended Breakpad information is present, this field will be set to -1,
  // indicating that the dump thread is not available.
  int requesting_thread_;

  // Stacks for each thread (except possibly the exception handler
  // thread) at the time of the crash.
  vector<CallStack*> threads_;

  // OS and CPU information.
  SystemInfo system_info_;

  // The modules that were loaded into the process represented by the
  // ProcessState.
  const CodeModules *modules_;
};

}  // namespace google_breakpad

#endif  // GOOGLE_BREAKPAD_PROCESSOR_PROCESS_STATE_H__
