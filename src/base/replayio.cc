#include "src/base/replayio.h"

#include "src/objects/string.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/handles-inl.h"
#include "src/strings/string-builder.h"  // If needed for string manipulation
#include "include/replayio.h"            // For AutoDisallowEvents and recordreplay helpers

namespace v8 {
namespace replayio {

AutoMaybeDisallowEvents::AutoMaybeDisallowEvents(bool disallowEvents, const char* label) {
  if (disallowEvents) {
    disallow.emplace(label);
  }
}

v8::internal::Handle<v8::internal::String> RecordReplayStringHandle(
    const char* why, v8::internal::Isolate* isolate,
    v8::internal::Handle<v8::internal::String> input) {
  if (!v8::recordreplay::IsRecordingOrReplaying(why)) {
    return input;
  }
  std::string str = input->ToCString().get();
  v8::recordreplay::RecordReplayString(why, str);
  return isolate->factory()->NewStringFromUtf8(base::CStrVector(str.c_str())).ToHandleChecked();
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
