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

int GetReplayObjectId(Isolate* isolate, Handle<Object> object,
                      bool allow_create) {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  return RecordReplayObjectId(v8_isolate, v8_isolate->GetCurrentContext(),
                              v8::Utils::ToLocal(object), allow_create);
}

struct ReplayWeakRefTarget {
  int weak_ref_id;
  v8::Global<v8::Value> target;
};

std::vector<ReplayWeakRefTarget>* gReplayWeakRefTargets;

int GetReplayWeakRefId(Isolate* isolate, Handle<JSWeakRef> weak_ref,
                       bool allow_create) {
  return GetReplayObjectId(isolate, weak_ref, allow_create);
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

struct ReplayFinalizationRegistry {
  int registry_id;
  v8::Global<v8::Value> registry;
};

struct ReplayFinalizationCell {
  int weak_cell_id;
  int registry_id;
  v8::Global<v8::Value> weak_cell;
  v8::Global<v8::Value> holdings;
};

std::vector<ReplayFinalizationRegistry>* gReplayFinalizationRegistries;
std::vector<ReplayFinalizationCell>* gReplayFinalizationCells;

int GetReplayFinalizationRegistryId(Isolate* isolate,
                                    Handle<JSFinalizationRegistry> registry,
                                    bool allow_create) {
  return GetReplayObjectId(isolate, registry, allow_create);
}

int GetReplayWeakCellId(Isolate* isolate, Handle<WeakCell> weak_cell,
                        bool allow_create) {
  return GetReplayObjectId(isolate, weak_cell, allow_create);
}

ReplayFinalizationRegistry* FindReplayFinalizationRegistry(int registry_id) {
  if (!gReplayFinalizationRegistries) {
    return nullptr;
  }
  for (ReplayFinalizationRegistry& entry : *gReplayFinalizationRegistries) {
    if (entry.registry_id == registry_id) {
      return &entry;
    }
  }
  return nullptr;
}

ReplayFinalizationCell* FindReplayFinalizationCell(int weak_cell_id) {
  if (!gReplayFinalizationCells) {
    return nullptr;
  }
  for (ReplayFinalizationCell& entry : *gReplayFinalizationCells) {
    if (entry.weak_cell_id == weak_cell_id) {
      return &entry;
    }
  }
  return nullptr;
}

bool HasReplayFinalizationCellsForRegistry(int registry_id) {
  if (!gReplayFinalizationCells) {
    return false;
  }
  for (const ReplayFinalizationCell& entry : *gReplayFinalizationCells) {
    if (entry.registry_id == registry_id) {
      return true;
    }
  }
  return false;
}

void RegisterReplayFinalizationRegistry(
    Isolate* isolate, Handle<JSFinalizationRegistry> registry) {
  int registry_id = GetReplayFinalizationRegistryId(isolate, registry, true);
  if (!registry_id) {
    return;
  }
  if (!gReplayFinalizationRegistries) {
    gReplayFinalizationRegistries = new std::vector<ReplayFinalizationRegistry>();
  }

  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  if (ReplayFinalizationRegistry* entry =
          FindReplayFinalizationRegistry(registry_id)) {
    entry->registry.Reset(v8_isolate, v8::Utils::ToLocal(registry));
    return;
  }

  ReplayFinalizationRegistry entry{registry_id, v8::Global<v8::Value>()};
  entry.registry.Reset(v8_isolate, v8::Utils::ToLocal(registry));
  gReplayFinalizationRegistries->push_back(std::move(entry));
}

Handle<JSFinalizationRegistry> LookupReplayFinalizationRegistry(
    Isolate* isolate, int registry_id) {
  ReplayFinalizationRegistry* entry =
      FindReplayFinalizationRegistry(registry_id);
  if (!entry) {
    return Handle<JSFinalizationRegistry>();
  }

  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  Local<v8::Value> value = entry->registry.Get(v8_isolate);
  if (value.IsEmpty()) {
    return Handle<JSFinalizationRegistry>();
  }
  return Handle<JSFinalizationRegistry>::cast(Utils::OpenHandle(*value));
}

void RegisterReplayFinalizationCell(Isolate* isolate,
                                    Handle<JSFinalizationRegistry> registry,
                                    Handle<WeakCell> weak_cell,
                                    Handle<Object> holdings) {
  int registry_id = GetReplayFinalizationRegistryId(isolate, registry, true);
  int weak_cell_id = GetReplayWeakCellId(isolate, weak_cell, true);
  if (!registry_id || !weak_cell_id) {
    return;
  }

  RegisterReplayFinalizationRegistry(isolate, registry);

  if (!gReplayFinalizationCells) {
    gReplayFinalizationCells = new std::vector<ReplayFinalizationCell>();
  }

  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  if (ReplayFinalizationCell* entry = FindReplayFinalizationCell(weak_cell_id)) {
    entry->weak_cell.Reset(v8_isolate, v8::Utils::ToLocal(weak_cell));
    entry->holdings.Reset(v8_isolate, v8::Utils::ToLocal(holdings));
    entry->registry_id = registry_id;
    return;
  }

  ReplayFinalizationCell entry{
      weak_cell_id, registry_id, v8::Global<v8::Value>(), v8::Global<v8::Value>()};
  entry.weak_cell.Reset(v8_isolate, v8::Utils::ToLocal(weak_cell));
  entry.holdings.Reset(v8_isolate, v8::Utils::ToLocal(holdings));
  gReplayFinalizationCells->push_back(std::move(entry));
}

Handle<WeakCell> LookupReplayWeakCell(Isolate* isolate, int weak_cell_id) {
  ReplayFinalizationCell* entry = FindReplayFinalizationCell(weak_cell_id);
  if (!entry) {
    return Handle<WeakCell>();
  }

  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  Local<v8::Value> value = entry->weak_cell.Get(v8_isolate);
  if (value.IsEmpty()) {
    return Handle<WeakCell>();
  }
  return Handle<WeakCell>::cast(Utils::OpenHandle(*value));
}

Handle<Object> LookupReplayFinalizationHoldings(Isolate* isolate,
                                                int weak_cell_id) {
  ReplayFinalizationCell* entry = FindReplayFinalizationCell(weak_cell_id);
  if (!entry) {
    return Handle<Object>();
  }

  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  Local<v8::Value> value = entry->holdings.Get(v8_isolate);
  if (value.IsEmpty()) {
    return Handle<Object>();
  }
  return Utils::OpenHandle(*value);
}

void ReleaseReplayFinalizationCell(int weak_cell_id) {
  if (!gReplayFinalizationCells) {
    return;
  }

  for (auto it = gReplayFinalizationCells->begin();
       it != gReplayFinalizationCells->end(); ++it) {
    if (it->weak_cell_id == weak_cell_id) {
      it->weak_cell.Reset();
      it->holdings.Reset();
      gReplayFinalizationCells->erase(it);
      return;
    }
  }
}

void MaybeReleaseReplayFinalizationRegistry(Isolate* isolate,
                                            Handle<JSFinalizationRegistry> registry) {
  int registry_id = GetReplayFinalizationRegistryId(isolate, registry, false);
  if (!registry_id || HasReplayFinalizationCellsForRegistry(registry_id) ||
      !gReplayFinalizationRegistries) {
    return;
  }

  for (auto it = gReplayFinalizationRegistries->begin();
       it != gReplayFinalizationRegistries->end(); ++it) {
    if (it->registry_id == registry_id) {
      it->registry.Reset();
      gReplayFinalizationRegistries->erase(it);
      return;
    }
  }
}

Handle<WeakCell> PopClearedCell(Isolate* isolate,
                                Handle<JSFinalizationRegistry> registry) {
  if (!registry->cleared_cells().IsWeakCell()) {
    return Handle<WeakCell>();
  }

  Handle<WeakCell> weak_cell(WeakCell::cast(registry->cleared_cells()), isolate);
  Object weak_cell_tail = weak_cell->next();
  registry->set_cleared_cells(weak_cell_tail);
  weak_cell->set_next(ReadOnlyRoots(isolate).undefined_value());
  if (weak_cell_tail.IsWeakCell()) {
    WeakCell tail_is_now_a_head = WeakCell::cast(weak_cell_tail);
    tail_is_now_a_head.set_prev(ReadOnlyRoots(isolate).undefined_value());
  }

  if (!weak_cell->unregister_token().IsUndefined(isolate)) {
    JSFinalizationRegistry::RemoveCellFromUnregisterTokenMap(
        isolate, registry->ptr(), weak_cell->ptr());
  }
  return weak_cell;
}

void DiscardClearedCells(Isolate* isolate,
                         Handle<JSFinalizationRegistry> registry) {
  while (!PopClearedCell(isolate, registry).is_null()) {
  }
}

void ShrinkFinalizationRegistryUnregisterTokenMap(
    Isolate* isolate, Handle<JSFinalizationRegistry> finalization_registry) {
  if (!finalization_registry->key_map().IsUndefined(isolate)) {
    Handle<SimpleNumberDictionary> key_map =
        handle(SimpleNumberDictionary::cast(finalization_registry->key_map()),
               isolate);
    key_map = SimpleNumberDictionary::Shrink(isolate, key_map);
    finalization_registry->set_key_map(*key_map);
  }
}

}  // namespace

Handle<JSFinalizationRegistry> LookupReplayFinalizationRegistryById(
    Isolate* isolate, int registry_id) {
  return LookupReplayFinalizationRegistry(isolate, registry_id);
}

int GetReplayFinalizationRegistryIdForTask(
    Isolate* isolate, Handle<JSFinalizationRegistry> registry,
    bool allow_create) {
  return GetReplayFinalizationRegistryId(isolate, registry, allow_create);
}

RUNTIME_FUNCTION(Runtime_ShrinkFinalizationRegistryUnregisterTokenMap) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<JSFinalizationRegistry> finalization_registry =
      args.at<JSFinalizationRegistry>(0);

  ShrinkFinalizationRegistryUnregisterTokenMap(isolate, finalization_registry);

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

RUNTIME_FUNCTION(Runtime_JSFinalizationRegistryRegisterForReplay) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  Handle<JSFinalizationRegistry> finalization_registry =
      args.at<JSFinalizationRegistry>(0);
  Handle<WeakCell> weak_cell = args.at<WeakCell>(1);
  Handle<Object> holdings = args.at(2);

  if (recordreplay::IsRecordingOrReplaying("weak-refs", "finalization-registry")) {
    GetReplayFinalizationRegistryId(isolate, finalization_registry, true);
    GetReplayWeakCellId(isolate, weak_cell, true);
  }
  if (recordreplay::IsReplaying("weak-refs", "finalization-registry")) {
    RegisterReplayFinalizationCell(isolate, finalization_registry, weak_cell,
                                   holdings);
  }

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

RUNTIME_FUNCTION(Runtime_JSFinalizationRegistryCleanupLoopForReplay) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<JSFinalizationRegistry> finalization_registry =
      args.at<JSFinalizationRegistry>(0);
  Handle<Object> callback = args.at(1);
  Handle<Object> undefined = isolate->factory()->undefined_value();

  if (recordreplay::IsRecordingOrReplaying("weak-refs",
                                           "finalization-registry")) {
    while (true) {
      int weak_cell_id = 0;
      if (recordreplay::IsReplaying("weak-refs", "finalization-registry")) {
        weak_cell_id = static_cast<int>(
            recordreplay::RecordReplayValue("JSFinalizationRegistry::CellId",
                                            0));
        if (!weak_cell_id) {
          break;
        }

        Handle<WeakCell> weak_cell =
            LookupReplayWeakCell(isolate, weak_cell_id);
        if (!weak_cell.is_null()) {
          if (!weak_cell->unregister_token().IsUndefined(isolate)) {
            JSFinalizationRegistry::RemoveCellFromUnregisterTokenMap(
                isolate, finalization_registry->ptr(), weak_cell->ptr());
          }
          weak_cell->RemoveFromFinalizationRegistryCells(isolate);
        }

        Handle<Object> holdings =
            LookupReplayFinalizationHoldings(isolate, weak_cell_id);
        CHECK(!holdings.is_null());
        Handle<Object> argv[] = {holdings};
        if (Execution::Call(isolate, Handle<JSReceiver>::cast(callback),
                            undefined, 1, argv)
                .is_null()) {
          ShrinkFinalizationRegistryUnregisterTokenMap(isolate,
                                                       finalization_registry);
          return ReadOnlyRoots(isolate).exception();
        }
        ReleaseReplayFinalizationCell(weak_cell_id);
        MaybeReleaseReplayFinalizationRegistry(isolate, finalization_registry);
      } else {
        Handle<WeakCell> weak_cell = PopClearedCell(isolate, finalization_registry);
        weak_cell_id = weak_cell.is_null()
                           ? 0
                           : GetReplayWeakCellId(isolate, weak_cell, false);
        weak_cell_id = static_cast<int>(recordreplay::RecordReplayValue(
            "JSFinalizationRegistry::CellId", weak_cell_id));
        if (!weak_cell_id) {
          break;
        }

        CHECK(!weak_cell.is_null());
        Handle<Object> argv[] = {Handle<Object>(weak_cell->holdings(), isolate)};
        if (Execution::Call(isolate, Handle<JSReceiver>::cast(callback),
                            undefined, 1, argv)
                .is_null()) {
          ShrinkFinalizationRegistryUnregisterTokenMap(isolate,
                                                       finalization_registry);
          return ReadOnlyRoots(isolate).exception();
        }
        ReleaseReplayFinalizationCell(weak_cell_id);
        MaybeReleaseReplayFinalizationRegistry(isolate, finalization_registry);
      }
    }

    if (recordreplay::IsReplaying("weak-refs", "finalization-registry")) {
      DiscardClearedCells(isolate, finalization_registry);
    }
    ShrinkFinalizationRegistryUnregisterTokenMap(isolate, finalization_registry);
    return ReadOnlyRoots(isolate).undefined_value();
  }

  while (true) {
    Handle<WeakCell> weak_cell = PopClearedCell(isolate, finalization_registry);
    if (weak_cell.is_null()) {
      break;
    }
    Handle<Object> argv[] = {Handle<Object>(weak_cell->holdings(), isolate)};
    if (Execution::Call(isolate, Handle<JSReceiver>::cast(callback), undefined,
                        1, argv)
            .is_null()) {
      ShrinkFinalizationRegistryUnregisterTokenMap(isolate,
                                                   finalization_registry);
      return ReadOnlyRoots(isolate).exception();
    }
  }

  ShrinkFinalizationRegistryUnregisterTokenMap(isolate, finalization_registry);
  return ReadOnlyRoots(isolate).undefined_value();
}

}  // namespace internal
}  // namespace v8
