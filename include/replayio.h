// Copyright (c) 2024 Record Replay Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API for interacting with the record/replay driver.
// Some parts are still in v8.h and still need to be migrated.


#ifndef INCLUDE_RECORD_REPLAY_H_
#define INCLUDE_RECORD_REPLAY_H_

#include "v8.h"

namespace v8 {
namespace replayio {
  
struct AutoPassThroughEvents {
  AutoPassThroughEvents() { v8::recordreplay::BeginPassThroughEvents(); }
  ~AutoPassThroughEvents() { v8::recordreplay::EndPassThroughEvents(); }
};

struct AutoDisallowEvents {
  AutoDisallowEvents() { v8::recordreplay::BeginDisallowEvents(); }
  AutoDisallowEvents(const char* label) { v8::recordreplay::BeginDisallowEventsWithLabel(label); }
  ~AutoDisallowEvents() { v8::recordreplay::EndDisallowEvents(); }
};

/** ###########################################################################
 * ReplayRootContext
 * ##########################################################################*/

/**
 * 
 */
class ReplayRootContext {
  static constexpr const char* FunctionCallRegisteredCallback = "CallRegisteredCallback";
  // static constexpr const char* FunctionCallRegisteredCallback = "CallRegisteredCallback";

  Eternal<Context> context_;
  /**
   * Internally used callback object we use for routing/registering JS callbacks.
   */
  Eternal<Object> callbackRegistry_;

public:
  Eternal<Context> Context();

  Local<v8::Function> GetFunction(
    Local<Object> object,
    const std::string& propName
  );

  Local<Value> CallFunction(Local<v8::Function> fn,
                            int argc,
                            Local<Value> argv[] = { });

  Local<Value> CallGlobalFunction(const std::string& functionName,
                                  int argc,
                                  Local<Value> argv[] = { });

  Local<Value> CallRegisteredCallback(const std::string& callbackName,
                                      Local<Object> param1);
};

ReplayRootContext* RecordReplayCreateRootContext(v8::Isolate* isolate, v8::Local<v8::Context> cx);

}  // namespace replayio
}  // namespace v8

#endif  // INCLUDE_RECORD_REPLAY_H_
