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
  Eternal<v8::Context> context_;
  /**
   * We use this to emit events into JS.
   */
  Eternal<Object> eventEmitter_;

public:
  ReplayRootContext(Eternal<v8::Context> context, Eternal<Object> eventEmitter) :
    context_(context),
    eventEmitter_(eventEmitter)
    {}

  Eternal<Context> ContextEternal() const { return context_; }
  Local<Context> GetContext() const;
  Local<Object> GetEventEmitter() const;

  Local<v8::Function> GetFunction(
    Local<Object> object,
    const std::string& propName
  ) const;

  Local<Value> CallFunction(Local<v8::Function> fn,
                            int argc = 0,
                            Local<Value> argv[] = nullptr,
                            MaybeLocal<Value> receiver = MaybeLocal<Value>()) const;

  Local<Value> CallGlobalFunction(const std::string& functionName,
                                  int argc = 0,
                                  Local<Value> argv[] = nullptr) const;

  Local<Value> EmitReplayEvent(const std::string& eventName,
                               Local<Value> param1) const;
  Local<Value> EmitReplayEvent(const std::string& eventName,
                               int argc = 0,
                               Local<Value> argv[] = nullptr) const;
};

ReplayRootContext* RecordReplayCreateRootContext(v8::Isolate* isolate, v8::Local<v8::Context> cx);

/**
 * @return The |ReplayRootContext| for the given |cx|.
 */
ReplayRootContext* RecordReplayGetRootContext(v8::Local<v8::Context> cx);

/**
 * @deprecated There is no single "default context".
 * @return The "default" |ReplayRootContext|.
 */
ReplayRootContext* RecordReplayGetRootContext();

/**
 * @return Whether any root context has been created yet.
 */
bool RecordReplayHasDefaultContext();

}  // namespace replayio
}  // namespace v8

#endif  // INCLUDE_RECORD_REPLAY_H_
