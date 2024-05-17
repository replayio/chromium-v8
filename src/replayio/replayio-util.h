// Copyright (c) 2024 Record Replay Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains all kinds of utilities used in Replay code.

#ifndef V8_REPLAY_REPLAYIO_UTIL_H
#define V8_REPLAY_REPLAYIO_UTIL_H

#include "src/api/api-inl.h"

namespace v8 {
namespace replayio {

internal::Handle<internal::String> CStringToHandle(internal::Isolate* isolate, const char* str);

Local<String> CStringToLocal(Isolate* isolate, const char* str);

internal::Handle<Object> GetProperty(internal::Isolate* isolate,
                                     internal::Handle<Object> obj, const char* property);

void SetProperty(internal::Isolate* isolate,
                 internal::Handle<Object> obj, const char* property,
                 internal::Handle<Object> value);

void SetProperty(internal::Isolate* isolate,
                 internal::Handle<Object> obj, const char* property,
                 const char* value);

void SetProperty(internal::Isolate* isolate,
                 internal::Handle<Object> obj, const char* property,
                 double value);

internal::Handle<internal::JSObject> NewPlainObject(internal::Isolate* isolate);

void CHECKIsJSFunction();

}  // namespace replayio
}  // namespace v8

#endif  // V8_REPLAY_REPLAYIO_UTIL_H
