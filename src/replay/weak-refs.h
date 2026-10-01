#ifndef V8_REPLAY_WEAK_REFS_H_
#define V8_REPLAY_WEAK_REFS_H_

#include "src/handles/handles.h"

namespace v8 {
namespace internal {
class Isolate;
class JSWeakRef;
}  // namespace internal

namespace replayio {

// Lets a replaying process release WeakRef targets the recording's GC
// collected.
//
// When replaying, the GC keeps the target of every WeakRef alive, and deref()
// returns what it returned when recording. A target would therefore stay
// alive for as long as its WeakRef does, even though the recording collected
// it. To avoid that, the recording describes which WeakRefs its GC cleared,
// and the replay clears the same ones.
//
// Only WeakRefs constructed at a point which replays are described this way
// ("tracked", JSWeakRef::record_replay_id != 0).
class ReplayWeakRefs {
 public:
  // Whether the "weak-ref-collection" feature is active. When it is not, no
  // WeakRef is tracked.
  static bool Enabled();

  static void OnConstruct(internal::Isolate* isolate,
                          internal::Handle<internal::JSWeakRef> weak_ref);

  // Called by the GC after it cleared the target of |weak_ref|.
  static void OnTargetCleared(internal::Isolate* isolate,
                              internal::JSWeakRef weak_ref);

  // Records/replays which tracked WeakRefs the recording's GC cleared since
  // the last poll. Called at the end of every microtask checkpoint.
  static void Poll(internal::Isolate* isolate);
};

}  // namespace replayio
}  // namespace v8

#endif  // V8_REPLAY_WEAK_REFS_H_
