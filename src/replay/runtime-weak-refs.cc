#include "include/replayio.h"
#include "src/execution/arguments-inl.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/replay/weak-refs.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

// Called from WeakRef constructor. During replay, pin the target so the GC
// cannot collect it earlier than it was collected during recording.
RUNTIME_FUNCTION(Runtime_JSReplayWeakRefConstruct) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<HeapObject> object = args.at<HeapObject>(0);

  if (recordreplay::IsReplaying()) {
    replayio::ReplayWeakRefPins::Pin(isolate, *object);
  }

  return ReadOnlyRoots(isolate).undefined_value();
}

// Called from WeakRef.prototype.deref(). Records/replays target liveness so
// the result is deterministic. Unpins the target once it is observed dead.
RUNTIME_FUNCTION(Runtime_JSReplayWeakRefDeref) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<JSWeakRef> weak_ref = args.at<JSWeakRef>(0);

  Object target = weak_ref->target();
  uintptr_t alive = target.IsUndefined(isolate) ? 0 : 1;
  alive = recordreplay::RecordReplayValue("JSWeakRef.deref", alive);

  if (!alive) {
    if (!target.IsUndefined(isolate)) {
      replayio::ReplayWeakRefPins::Unpin(isolate, HeapObject::cast(target));
    }
    return ReadOnlyRoots(isolate).undefined_value();
  }

  return target;
}

}  // namespace internal
}  // namespace v8
