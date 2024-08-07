// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ELEMENTS_INL_H_
#define V8_OBJECTS_ELEMENTS_INL_H_

#include "src/common/globals.h"
#include "src/objects/elements.h"

#include "src/handles/handles-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

V8_WARN_UNUSED_RESULT inline ExceptionStatus
ElementsAccessor::CollectElementIndices(Handle<JSObject> object,
                                        KeyAccumulator* keys) {
  return CollectElementIndices(
      object, handle(object->elements(), keys->isolate()), keys);
}

inline MaybeHandle<FixedArray> ElementsAccessor::PrependElementIndices(
    Isolate* isolate, Handle<JSObject> object, Handle<FixedArray> keys,
    GetKeysConversion convert, PropertyFilter filter,
    const KeyIterationParams* params) {
  return PrependElementIndices(isolate, object,
                               handle(object->elements(), isolate), keys,
                               convert, filter, params);
}

inline bool ElementsAccessor::HasElement(JSObject holder, uint32_t index,
                                         PropertyFilter filter) {
  return HasElement(holder, index, holder.elements(), filter);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_ELEMENTS_INL_H_
