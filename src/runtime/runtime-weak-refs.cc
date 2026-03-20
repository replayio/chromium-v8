// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "src/api/api-inl.h"
#include "src/execution/arguments-inl.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

extern int RecordReplayObjectId(v8::Isolate* isolate, v8::Local<v8::Context> cx,
                                v8::Local<v8::Value> object, bool allow_create);

namespace {

struct ReplayWeakRefTarget {
  int weak_ref_id;
  v8::Global<v8::Value> target;
};

std::vector<ReplayWeakRefTarget>* gReplayWeakRefTargets;

int GetReplayWeakRefId(Isolate* isolate, Handle<JSWeakRef> weak_ref,
                       bool allow_create) {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  return RecordReplayObjectId(v8_isolate, v8_isolate->GetCurrentContext(),
                              v8::Utils::ToLocal(weak_ref), allow_create);
}

ReplayWeakRefTarget* FindReplayWeakRefTarget(int weak_ref_id) {
  if (!gReplayWeakRefTargets) {
    return nullptr;
  }

  for (ReplayWeakRefTarget& entry : *gReplayWeakRefTargets) {
    if (entry.weak_ref_id == weak_ref_id) {
      return &entry;
    }
  }
  return nullptr;
}

void RegisterReplayWeakRefTarget(Isolate* isolate, Handle<JSWeakRef> weak_ref,
                                 Handle<HeapObject> target) {
  int weak_ref_id = GetReplayWeakRefId(isolate, weak_ref, true);
  if (!weak_ref_id) {
    return;
  }

  if (!gReplayWeakRefTargets) {
    gReplayWeakRefTargets = new std::vector<ReplayWeakRefTarget>();
  }

  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  if (ReplayWeakRefTarget* entry = FindReplayWeakRefTarget(weak_ref_id)) {
    entry->target.Reset(v8_isolate, v8::Utils::ToLocal(target));
    return;
  }

  ReplayWeakRefTarget entry{weak_ref_id, v8::Global<v8::Value>()};
  entry.target.Reset(v8_isolate, v8::Utils::ToLocal(target));
  gReplayWeakRefTargets->push_back(std::move(entry));
}

Handle<Object> LookupReplayWeakRefTarget(Isolate* isolate,
                                         Handle<JSWeakRef> weak_ref) {
  int weak_ref_id = GetReplayWeakRefId(isolate, weak_ref, false);
  if (!weak_ref_id) {
    return Handle<Object>();
  }

  ReplayWeakRefTarget* entry = FindReplayWeakRefTarget(weak_ref_id);
  if (!entry) {
    return Handle<Object>();
  }

  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  Local<v8::Value> value = entry->target.Get(v8_isolate);
  if (value.IsEmpty()) {
    return Handle<Object>();
  }
  return Utils::OpenHandle(*value);
}

void ReleaseReplayWeakRefTarget(Isolate* isolate, Handle<JSWeakRef> weak_ref) {
  int weak_ref_id = GetReplayWeakRefId(isolate, weak_ref, false);
  if (!weak_ref_id || !gReplayWeakRefTargets) {
    return;
  }

  for (auto it = gReplayWeakRefTargets->begin();
       it != gReplayWeakRefTargets->end(); ++it) {
    if (it->weak_ref_id == weak_ref_id) {
      it->target.Reset();
      gReplayWeakRefTargets->erase(it);
      return;
    }
  }
}

}  // namespace

RUNTIME_FUNCTION(Runtime_ShrinkFinalizationRegistryUnregisterTokenMap) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<JSFinalizationRegistry> finalization_registry =
      args.at<JSFinalizationRegistry>(0);

  if (!finalization_registry->key_map().IsUndefined(isolate)) {
    Handle<SimpleNumberDictionary> key_map =
        handle(SimpleNumberDictionary::cast(finalization_registry->key_map()),
               isolate);
    key_map = SimpleNumberDictionary::Shrink(isolate, key_map);
    finalization_registry->set_key_map(*key_map);
  }

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(
    Runtime_JSFinalizationRegistryRegisterWeakCellWithUnregisterToken) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<JSFinalizationRegistry> finalization_registry =
      args.at<JSFinalizationRegistry>(0);
  Handle<WeakCell> weak_cell = args.at<WeakCell>(1);

  JSFinalizationRegistry::RegisterWeakCellWithUnregisterToken(
      finalization_registry, weak_cell, isolate);

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_JSWeakRefAddToKeptObjects) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<HeapObject> object = args.at<HeapObject>(0);
  DCHECK(object->CanBeHeldWeakly());

  isolate->heap()->KeepDuringJob(object);

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_JSWeakRefRegisterTargetForReplay) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<JSWeakRef> weak_ref = args.at<JSWeakRef>(0);
  Handle<HeapObject> target = args.at<HeapObject>(1);
  DCHECK(target->CanBeHeldWeakly());

  if (recordreplay::IsRecordingOrReplaying("weak-refs")) {
    GetReplayWeakRefId(isolate, weak_ref, true);
  }
  if (recordreplay::IsReplaying("weak-refs")) {
    RegisterReplayWeakRefTarget(isolate, weak_ref, target);
  }

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_JSWeakRefDerefForReplay) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<JSWeakRef> weak_ref = args.at<JSWeakRef>(0);
  Handle<Object> target(weak_ref->target(), isolate);
  bool is_live = !target->IsUndefined(isolate);

  if (recordreplay::IsRecordingOrReplaying("weak-refs")) {
    is_live = !!recordreplay::RecordReplayValue("JSWeakRef::deref", is_live);
    if (!is_live) {
      ReleaseReplayWeakRefTarget(isolate, weak_ref);
      return ReadOnlyRoots(isolate).undefined_value();
    }

    if (target->IsUndefined(isolate)) {
      target = LookupReplayWeakRefTarget(isolate, weak_ref);
      CHECK(!target.is_null() && !target->IsUndefined(isolate));
    }
  }

  DCHECK(target->IsHeapObject());
  isolate->heap()->KeepDuringJob(Handle<HeapObject>::cast(target));
  return *target;
}

}  // namespace internal
}  // namespace v8
