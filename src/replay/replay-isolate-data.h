#ifndef V8_REPLAY_REPLAY_ISOLATE_DATA_H_
#define V8_REPLAY_REPLAY_ISOLATE_DATA_H_

#include <unordered_map>
#include <vector>

#include "include/v8-persistent-handle.h"

namespace v8 {
namespace replayio {

// General-purpose per-Isolate data for recording and replaying.
class ReplayIsolateData {
 public:
  ReplayIsolateData() = default;
  ~ReplayIsolateData() = default;

  ReplayIsolateData(const ReplayIsolateData&) = delete;
  ReplayIsolateData& operator=(const ReplayIsolateData&) = delete;

  std::vector<v8::Global<v8::Value>>& weak_ref_pins() { return weak_ref_pins_; }

  int NewFinalizationRegistryId() { return next_finalization_registry_id_++; }
  int NewWeakCellId() { return next_weak_cell_id_++; }

  bool finalization_registry_poll_armed() const {
    return finalization_registry_poll_armed_;
  }
  void ArmFinalizationRegistryPoll() {
    finalization_registry_poll_armed_ = true;
  }

  bool finalization_registry_task_posted() const {
    return finalization_registry_task_posted_;
  }
  void set_finalization_registry_task_posted(bool posted) {
    finalization_registry_task_posted_ = posted;
  }

  // While replaying, the tracked registries with registered cells, by
  // JSFinalizationRegistry::replay_id.
  std::unordered_map<int, v8::Global<v8::Value>>& finalization_registries() {
    return finalization_registries_;
  }

 private:
  std::vector<v8::Global<v8::Value>> weak_ref_pins_;

  int next_finalization_registry_id_ = 1;
  int next_weak_cell_id_ = 1;

  // Set by the first FinalizationRegistry.prototype.register() call on a
  // tracked registry and never cleared. While unset, the ClearKeptObjects poll
  // records nothing.
  //
  // A count of outstanding registrations could disarm the poll again and
  // shrink recordings. To be correct it would have to:
  //   - decrement on callback delivery and on unregister() (per removed cell),
  //   - never decrement when the GC collects a registry, since replay cannot
  //     observe that,
  //   - ignore cells registered while events are disallowed or after diverging.
  // A mismatch desyncs the recorded value stream. Typical users keep
  // registrations outstanding for the life of the page, so the count would
  // rarely return to zero and is unlikely to be worth it.
  bool finalization_registry_poll_armed_ = false;

  // Whether a cleanup task for tracked registries is posted and has not run.
  bool finalization_registry_task_posted_ = false;

  std::unordered_map<int, v8::Global<v8::Value>> finalization_registries_;
};

}  // namespace replayio
}  // namespace v8

#endif  // V8_REPLAY_REPLAY_ISOLATE_DATA_H_
