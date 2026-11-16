#include <vector>

#include "src/base/strings.h"
#include "src/base/vector.h"
#include "src/execution/isolate-inl.h"
#include "src/heap/factory.h"
#include "src/objects/string-inl.h"
#include "src/objects/string.h"
#include "include/replayio.h"
#include "src/replay/replayio.h"

namespace v8 {
namespace replayio {

bool AreEventsAvailable() {
  return !recordreplay::AreEventsDisallowed() &&
         !recordreplay::AreEventsPassedThrough() &&
         !recordreplay::HasDivergedFromRecording();
}

v8::internal::Handle<v8::internal::String> RecordReplayStringHandle(
    const char* why, v8::internal::Isolate* isolate,
    v8::internal::Handle<v8::internal::String> input) {
  if (!v8::recordreplay::IsRecordingOrReplaying(why)) {
    return input;
  }
  // The string's UTF-16 code units, so that the string made of them is the
  // input when recording and the recorded one when replaying. Going through
  // UTF-8 and a C string cut the string at a NUL and replaced an unpaired
  // surrogate, when recording too.
  std::vector<base::uc16> units(input->length());
  v8::internal::String::WriteToFlat(*input, units.data(), 0, input->length());
  size_t length = v8::recordreplay::RecordReplayValue(why, units.size());
  units.resize(length);
  if (length) {
    v8::recordreplay::RecordReplayBytes(why, units.data(),
                                        length * sizeof(base::uc16));
  }
  return isolate->factory()
      ->NewStringFromTwoByte(
          base::Vector<const base::uc16>(units.data(), units.size()))
      .ToHandleChecked();
}

v8::internal::MaybeHandle<v8::internal::String> RecordReplayStringHandle(
    const char* why, v8::internal::Isolate* isolate,
    v8::internal::MaybeHandle<v8::internal::String> input) {
  if (input.is_null()) {
    return input;
  }
  return RecordReplayStringHandle(why, isolate, input.ToHandleChecked());
}

}  // namespace replayio
}  // namespace v8
