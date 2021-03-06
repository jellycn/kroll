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
//
// This component uses the HTTPMultipartUpload of the breakpad project to send
// the minidump and associated data to the crash reporting servers.
// It will perform throttling based on the parameters passed to it and will
// prompt the user to send the minidump.

#include <Foundation/Foundation.h>

#include "client/mac/Framework/Breakpad.h"

#define kClientIdPreferenceKey @"clientid"

@interface Reporter : NSObject {
 @public
  IBOutlet NSWindow *alertWindow;       // The alert window

  // Values bound in the XIB
  NSString *headerMessage_;           // Message notifying of the crash
  NSString *reportMessage_;           // Message explaining the crash report
  NSString *commentsValue_;           // Comments from the user
  NSString *emailMessage_;            // Message requesting user email
  NSString *emailValue_;              // Email from the user

 @private
  int configFile_;                    // File descriptor for config file
  NSMutableDictionary *parameters_;   // Key value pairs of data (STRONG)
  NSData *minidumpContents_;          // The data in the minidump (STRONG)
  NSData *logFileData_;               // An NSdata for the tar, bz2'd log file
}

// Stops the modal panel with an NSAlertDefaultReturn value. This is the action
// invoked by the "Send Report" button.
- (IBAction)sendReport:(id)sender;
// Stops the modal panel with an NSAlertAlternateReturn value. This is the
// action invoked by the "Cancel" button.
- (IBAction)cancel:(id)sender;
// Opens the Google Privacy Policy in the default web browser.
- (IBAction)showPrivacyPolicy:(id)sender;

// Delegate methods for the NSTextField for comments. We want to capture the
// Return key and use it to send the message when no text has been entered.
// Otherwise, we want Return to add a carriage return to the comments field.
- (BOOL)control:(NSControl*)control textView:(NSTextView*)textView
                         doCommandBySelector:(SEL)commandSelector;

// Accessors to make bindings work
- (NSString *)headerMessage;
- (void)setHeaderMessage:(NSString *)value;

- (NSString *)reportMessage;
- (void)setReportMessage:(NSString *)value;

- (NSString *)commentsValue;
- (void)setCommentsValue:(NSString *)value;

- (NSString *)emailMessage;
- (void)setEmailMessage:(NSString *)value;

- (NSString *)emailValue;
- (void)setEmailValue:(NSString *)value;
@end
