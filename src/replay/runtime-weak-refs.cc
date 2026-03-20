#include "include/replayio.h"
#include "src/execution/arguments-inl.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/replay/weak-refs.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

// During replay, keep the WeakRef target strongly reachable for at least as long
// as it was during recording.
RUNTIME_FUNCTION(Runtime_JSWeakRefsReplayPin) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<HeapObject> object = args.at<HeapObject>(0);

  replayio::WeakRefsReplay::Pin(isolate, *object);

  return ReadOnlyRoots(isolate).undefined_value();
}

// WeakRef.deref liveness during replay is driven by the recording, not current
// heap state. If the recording says "dead", release the replay pin.
RUNTIME_FUNCTION(Runtime_JSWeakRefDerefReplay) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<JSWeakRef> weak_ref = args.at<JSWeakRef>(0);

  Object target = weak_ref->target();
  uintptr_t alive = target.IsUndefined(isolate) ? 0 : 1;
  alive = recordreplay::RecordReplayValue("JSWeakRef.deref", alive);

  if (!alive) {
    if (!target.IsUndefined(isolate)) {
      replayio::WeakRefsReplay::Release(isolate, HeapObject::cast(target));
    }
    return ReadOnlyRoots(isolate).undefined_value();
  }


  return target;
}

}  // namespace internal
}  // namespace v8
