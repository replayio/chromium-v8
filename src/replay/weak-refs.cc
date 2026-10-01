#include "src/replay/weak-refs.h"

#include <vector>

#include "include/replayio.h"
#include "src/execution/isolate.h"
#include "src/handles/global-handles-inl.h"
#include "src/heap/factory.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/replay/replay-isolate-data.h"
#include "src/replay/replayio.h"

namespace v8 {
namespace replayio {

namespace i = internal;

bool ReplayWeakRefs::Enabled() {
  return recordreplay::IsRecordingOrReplaying("weak-ref-collection");
}

void ReplayWeakRefs::OnConstruct(i::Isolate* isolate,
                                 i::Handle<i::JSWeakRef> weak_ref) {
  if (!Enabled() || !AreEventsAvailable()) return;

  ReplayIsolateData* data = isolate->EnsureReplayData();
  int id = data->NewWeakRefId();
  recordreplay::Assert("WeakRef.construct %d", id);
  weak_ref->set_record_replay_id(id);

  if (!recordreplay::IsReplaying()) return;

  // The ids are consecutive, so the WeakRef with a given id is at index
  // id - 1. The list holds them weakly.
  i::Handle<i::WeakArrayList> weak_refs;
  if (i::Address* location = data->tracked_weak_refs_location()) {
    weak_refs = i::Handle<i::WeakArrayList>(location);
  } else {
    weak_refs = isolate->factory()->empty_weak_array_list();
  }
  CHECK_EQ(weak_refs->length(), id - 1);
  i::Handle<i::WeakArrayList> new_weak_refs = i::WeakArrayList::AddToEnd(
      isolate, weak_refs, i::MaybeObjectHandle::Weak(weak_ref));
  if (i::Address* location = data->tracked_weak_refs_location()) {
    *location = new_weak_refs->ptr();
  } else {
    data->set_tracked_weak_refs_location(
        isolate->global_handles()->Create(*new_weak_refs).location());
  }
}

void ReplayWeakRefs::OnTargetCleared(i::Isolate* isolate,
                                     i::JSWeakRef weak_ref) {
  // Runs inside the GC, so this only notes the id for the next Poll().
  int id = weak_ref.record_replay_id();
  if (!id) return;
  isolate->replay_data()->cleared_weak_refs().push_back(id);
}

void ReplayWeakRefs::Poll(i::Isolate* isolate) {
  ReplayIsolateData* data = isolate->replay_data();
  if (!data || !data->has_tracked_weak_refs()) return;
  if (!AreEventsAvailable()) return;

  std::vector<int> cleared;
  if (recordreplay::IsRecording()) cleared.swap(data->cleared_weak_refs());
  size_t cleared_count =
      recordreplay::RecordReplayValue("WeakRef.cleared", cleared.size());
  if (!cleared_count) return;

  cleared.resize(cleared_count);
  recordreplay::RecordReplayBytes("WeakRef.cleared ids", cleared.data(),
                                  cleared_count * sizeof(int));
  if (!recordreplay::IsReplaying()) return;

  i::WeakArrayList weak_refs =
      i::WeakArrayList::cast(i::Object(*data->tracked_weak_refs_location()));
  for (int id : cleared) {
    i::HeapObject weak_ref;
    // The replay's GC may have collected the WeakRef itself already.
    if (weak_refs.Get(id - 1)->GetHeapObjectIfWeak(&weak_ref)) {
      i::JSWeakRef::cast(weak_ref).set_target(
          i::ReadOnlyRoots(isolate).undefined_value());
    }
  }
}

}  // namespace replayio
}  // namespace v8
