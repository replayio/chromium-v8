// Copyright (c) 2024 Record Replay Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API for interacting with the record/replay driver.
// Some parts are still in v8.h and still need to be migrated.


#ifndef INCLUDE_RECORD_REPLAY_H_
#define INCLUDE_RECORD_REPLAY_H_

#include "v8.h"
#include "src/handles/handles.h"

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
struct ReplayRootContext {
  Eternal<Context> context;
  /**
   * Internally used callback object we use for routing/registering JS callbacks.
   */
  Eternal<Object> callbackRegistry;
  
  Local<v8::Function> ReplayRootContext::GetFunction(
    Local<Object> object,
    const std::string& propName
  );

  i::Handle<i::Object> CallGlobalFunction(const std::string& functionName,
                                          int argc = 0,
                                          v8::internal::Handle<v8::internal::Object> argv[] = { });

  i::Handle<i::Object> CallRegisteredCallback(const std::string& callbackName,
                                              v8::internal::Handle<v8::internal::Object> paramsObj);
};

ReplayRootContext* RecordReplayCreateRootContext(v8::Isolate* isolate, v8::Local<v8::Context> cx);

}  // namespace replayio
}  // namespace v8

#endif  // INCLUDE_RECORD_REPLAY_H_
