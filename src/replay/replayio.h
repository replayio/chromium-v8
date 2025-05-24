#ifndef V8_REPLAYIO_STRINGS_H_
#define V8_REPLAYIO_STRINGS_H_

#include "src/handles/handles.h"
#include "src/handles/maybe-handles.h"

namespace v8 {
namespace internal {
class Isolate;
class String;
}  // namespace internal

namespace replayio {

v8::internal::Handle<v8::internal::String> RecordReplayStringHandle(
    const char* why, v8::internal::Isolate* isolate,
    v8::internal::Handle<v8::internal::String> input);

v8::internal::MaybeHandle<v8::internal::String> RecordReplayStringHandle(
    const char* why, v8::internal::Isolate* isolate,
    v8::internal::MaybeHandle<v8::internal::String> input);

}  // namespace replayio
}  // namespace v8

#endif  // V8_REPLAYIO_STRINGS_H_
