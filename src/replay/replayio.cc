#include <vector>

#include "src/execution/isolate-inl.h"
#include "src/objects/string.h"
#include "include/replayio.h"
#include "src/replay/replayio.h"

namespace v8 {
namespace replayio {

v8::internal::Handle<v8::internal::String> RecordReplayStringHandle(
    const char* why, v8::internal::Isolate* isolate,
    v8::internal::Handle<v8::internal::String> input) {
  if (!v8::recordreplay::IsRecordingOrReplaying(why)) {
    return input;
  }
  // Copying UTF-16 code units instead of converting to UTF-8 keeps NULs and
  // unpaired surrogates intact.
  std::vector<v8::base::uc16> units(input->length());
  v8::internal::String::WriteToFlat(*input, units.data(), 0, input->length());
  size_t length = v8::recordreplay::RecordReplayValue(why, units.size());
  units.resize(length);
  if (length) {
    v8::recordreplay::RecordReplayBytes(why, units.data(),
                                        length * sizeof(v8::base::uc16));
  }
  return isolate->factory()
      ->NewStringFromTwoByte(v8::base::Vector<const v8::base::uc16>(
          units.data(), static_cast<int>(units.size())))
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
