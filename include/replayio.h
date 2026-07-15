// Copyright (c) 2024 Record Replay Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API for interacting with the record/replay driver.
// Some parts are still in v8.h and still need to be migrated.


#ifndef INCLUDE_RECORD_REPLAY_H_
#define INCLUDE_RECORD_REPLAY_H_

#include <optional>

#include "v8.h"

namespace v8 {
namespace replayio {

struct AutoPassThroughEvents {
  AutoPassThroughEvents() { v8::recordreplay::BeginPassThroughEvents(); }
  ~AutoPassThroughEvents() { v8::recordreplay::EndPassThroughEvents(); }
};

struct AutoDisallowEvents {
  AutoDisallowEvents() { Begin(nullptr, nullptr); }
  explicit AutoDisallowEvents(const char* label, v8::Isolate* isolate = nullptr) {
    Begin(label, isolate);
  }
  ~AutoDisallowEvents() {
    no_js_.reset();
    v8::recordreplay::EndDisallowEvents();
  }

  AutoDisallowEvents(const AutoDisallowEvents&) = delete;
  AutoDisallowEvents& operator=(const AutoDisallowEvents&) = delete;

 private:
  void Begin(const char* label, v8::Isolate* isolate) {
    if (label) {
      v8::recordreplay::BeginDisallowEventsWithLabel(label);
    } else {
      v8::recordreplay::BeginDisallowEvents();
    }
    if (!isolate) {
      isolate = v8::Isolate::TryGetCurrent();
    }
    if (isolate) {
      no_js_.emplace(
          isolate,
          v8::Isolate::DisallowJavascriptExecutionScope::DUMP_ON_FAILURE);
    }
  }

  std::optional<v8::Isolate::DisallowJavascriptExecutionScope> no_js_;
};

}  // namespace replayio
}  // namespace v8

#endif  // INCLUDE_RECORD_REPLAY_H_
