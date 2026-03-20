// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8.h"
#include "src/heap/finalization-registry-cleanup-task.h"

#include "src/execution/frames.h"
#include "src/execution/interrupts-scope.h"
#include "src/execution/stack-guard.h"
#include "src/execution/v8threads.h"
#include "src/heap/heap-inl.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/tracing/trace-event.h"

namespace v8 {
namespace internal {

extern int GetReplayFinalizationRegistryIdForTask(
    Isolate* isolate, Handle<JSFinalizationRegistry> registry,
    bool allow_create);
extern Handle<JSFinalizationRegistry> LookupReplayFinalizationRegistryById(
    Isolate* isolate, int registry_id);

FinalizationRegistryCleanupTask::FinalizationRegistryCleanupTask(Heap* heap)
    : CancelableTask(heap->isolate()), heap_(heap) {}

void FinalizationRegistryCleanupTask::SlowAssertNoActiveJavaScript() {
#ifdef ENABLE_SLOW_DCHECKS
  class NoActiveJavaScript : public ThreadVisitor {
   public:
    void VisitThread(Isolate* isolate, ThreadLocalTop* top) override {
      for (StackFrameIterator it(isolate, top); !it.done(); it.Advance()) {
        DCHECK(!it.frame()->is_java_script());
      }
    }
  };
  NoActiveJavaScript no_active_js_visitor;
  Isolate* isolate = heap_->isolate();
  no_active_js_visitor.VisitThread(isolate, isolate->thread_local_top());
  isolate->thread_manager()->IterateArchivedThreads(&no_active_js_visitor);
#endif  // ENABLE_SLOW_DCHECKS
}

void FinalizationRegistryCleanupTask::RunInternal() {
  Isolate* isolate = heap_->isolate();
  SlowAssertNoActiveJavaScript();

  TRACE_EVENT_CALL_STATS_SCOPED(isolate, "v8",
                                "V8.FinalizationRegistryCleanupTask");

  HandleScope handle_scope(isolate);
  Handle<JSFinalizationRegistry> finalization_registry;
  // There could be no dirty FinalizationRegistries. When a context is disposed
  // by the embedder, its FinalizationRegistries are removed from the dirty
  // list.
  bool had_native_registry =
      heap_->DequeueDirtyJSFinalizationRegistry().ToHandle(&finalization_registry);
  int registry_id = 0;
  if (recordreplay::IsRecordingOrReplaying("weak-refs",
                                           "finalization-registry")) {
    if (had_native_registry) {
      registry_id =
          GetReplayFinalizationRegistryIdForTask(isolate, finalization_registry,
                                                true);
    }
    registry_id = static_cast<int>(recordreplay::RecordReplayValue(
        "JSFinalizationRegistry::TaskRegistry", registry_id));
    if (!had_native_registry && registry_id) {
      finalization_registry =
          LookupReplayFinalizationRegistryById(isolate, registry_id);
      had_native_registry = !finalization_registry.is_null();
    }
  }
  if (!had_native_registry) {
    return;
  }
  finalization_registry->set_scheduled_for_cleanup(false);

  // Since FinalizationRegistry cleanup callbacks are scheduled by V8, enter the
  // FinalizationRegistry's context.
  Handle<Context> context(
      Context::cast(finalization_registry->native_context()), isolate);
  Handle<Object> callback(finalization_registry->cleanup(), isolate);
  v8::Context::Scope context_scope(v8::Utils::ToLocal(context));
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  v8::TryCatch catcher(v8_isolate);
  catcher.SetVerbose(true);
  std::unique_ptr<MicrotasksScope> microtasks_scope;
  MicrotaskQueue* microtask_queue =
      finalization_registry->native_context().microtask_queue();
  if (!microtask_queue) microtask_queue = isolate->default_microtask_queue();
  if (microtask_queue &&
      microtask_queue->microtasks_policy() == v8::MicrotasksPolicy::kScoped) {
    // InvokeFinalizationRegistryCleanupFromTask will call into V8 API methods,
    // so we need a valid microtasks scope on the stack to avoid running into
    // the CallDepthScope check.
    microtasks_scope.reset(new v8::MicrotasksScope(
        v8_isolate, microtask_queue, v8::MicrotasksScope::kDoNotRunMicrotasks));
  }

  // Exceptions are reported via the message handler. This is ensured by the
  // verbose TryCatch.
  //
  // Cleanup is interrupted if there is an exception. The HTML spec calls for a
  // microtask checkpoint after each cleanup task, so the task should return
  // after an exception so the host can perform a microtask checkpoint. In case
  // of exception, check if the FinalizationRegistry still needs cleanup
  // and should be requeued.
  //
  // TODO(syg): Implement better scheduling for finalizers.
  InvokeFinalizationRegistryCleanupFromTask(context, finalization_registry,
                                            callback);
  if (finalization_registry->NeedsCleanup() &&
      !finalization_registry->scheduled_for_cleanup()) {
    auto nop = [](HeapObject, ObjectSlot, Object) {};
    heap_->EnqueueDirtyJSFinalizationRegistry(*finalization_registry, nop);
  }

  // Repost if there are remaining dirty FinalizationRegistries.
  heap_->set_is_finalization_registry_cleanup_task_posted(false);
  heap_->PostFinalizationRegistryCleanupTaskIfNeeded();
}

}  // namespace internal
}  // namespace v8
