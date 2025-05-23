// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is a clone of "base/optional.h" in chromium.
// Keep in sync, especially when fixing bugs.
// Copyright 2017 the V8 project authors. All rights reserved.

#ifndef V8_BASE_REPLAYIO_H
#define V8_BASE_REPLAYIO_H

#include "include/replayio.h"

#include "src/handles/handles.h"
#include "src/handles/maybe-handles.h"
#include "src/objects/string.h"
#include "src/base/optional.h"

namespace v8 {
namespace replayio {

using v8::internal::Handle;
using v8::internal::MaybeHandle;
using v8::internal::String;
using v8::internal::Isolate;

struct AutoMaybeDisallowEvents {
  AutoMaybeDisallowEvents(bool disallowEvents, const char* label) {
    if (disallowEvents) {
      disallow.emplace(label);
    }
  }
  
private:
  v8::base::Optional<v8::replayio::AutoDisallowEvents> disallow;
};

Handle<String> RecordReplayStringHandle(const char* why, Isolate* isolate, Handle<String> input) {
  if (!v8::recordreplay::IsRecordingOrReplaying(why)) {
    return input;
  }
  std::string str = input->ToCString().get();
  v8::recordreplay::RecordReplayString(why, str);
  return isolate->factory()->NewStringFromUtf8(base::CStrVector(str.c_str())).ToHandleChecked();
}

MaybeHandle<String> RecordReplayStringHandle(const char* why, Isolate* isolate, MaybeHandle<String> input) {
  if (input.is_null() || input.IsEmpty()) {
    return input;
  }
  return MaybeHandle<String>(RecordReplayStringHandle(why, isolate, input.ToHandleChecked()));
}

}  // namespace replayio
}  // namespace v8

#endif  // V8_BASE_REPLAYIO_H
