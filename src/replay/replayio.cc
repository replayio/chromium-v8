#include <vector>

#include "src/api/api-inl.h"
#include "src/base/replayio.h"
#include "src/base/strings.h"
#include "src/base/vector.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/messages.h"
#include "src/heap/factory-inl.h"
#include "src/heap/factory.h"
#include "src/objects/debug-objects-inl.h"
#include "src/objects/js-function-inl.h"
#include "src/objects/lookup-inl.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/objects/string-inl.h"
#include "src/objects/string.h"
#include "src/objects/struct-inl.h"
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

namespace {

// V8's own Error stack getter, as an Error's accessor pair holds it once the
// template it is installed from has been instantiated.
bool IsErrorStackGetter(v8::internal::Isolate* isolate,
                        v8::internal::Tagged<v8::internal::Object> getter) {
  if (!v8::internal::IsJSFunction(getter)) {
    return false;
  }
  v8::internal::Tagged<v8::internal::SharedFunctionInfo> shared =
      v8::internal::Cast<v8::internal::JSFunction>(getter)->shared();
  return shared->IsApiFunction() &&
         shared->api_func_data() ==
             *isolate->factory()->error_stack_getter_fun_template();
}

}  // namespace

bool ReadsFormattedErrorStack(v8::Local<v8::Object> object,
                              v8::Local<v8::Name> name) {
  v8::internal::Isolate* isolate = v8::internal::Isolate::Current();

  // Only an Error's own "stack". Checking the name first keeps the lookups
  // below off every other builtin getter a description walks.
  if (*Utils::OpenDirectHandle(*name) != *isolate->factory()->stack_string()) {
    return false;
  }
  v8::internal::DirectHandle<v8::internal::JSReceiver> receiver =
      Utils::OpenDirectHandle(*object);
  if (!v8::internal::IsJSObject(*receiver)) {
    return false;
  }
  v8::internal::DirectHandle<v8::internal::JSObject> error =
      v8::internal::Cast<v8::internal::JSObject>(receiver);

  // V8's own Error stack getter, as an own property, so that reading "stack" on
  // |object| really calls it: a page getter shadowing an inherited one, a proxy
  // trap, an interceptor and an access check all end the lookup elsewhere.
  v8::internal::LookupIterator it(isolate, error,
                                  isolate->factory()->stack_string(), error,
                                  v8::internal::LookupIterator::OWN);
  if (it.state() != v8::internal::LookupIterator::ACCESSOR) {
    return false;
  }
  v8::internal::DirectHandle<v8::internal::Object> accessors =
      it.GetAccessors();
  if (!v8::internal::IsAccessorPair(*accessors) ||
      !IsErrorStackGetter(
          isolate,
          v8::internal::Cast<v8::internal::AccessorPair>(*accessors)
              ->getter())) {
    return false;
  }

  // The formatted stack is already stored, so the getter's
  // ErrorUtils::GetFormattedStack hands that back instead of calling
  // FormatStackTrace, which is what reaches the page's
  // Error.prepareStackTrace.
  v8::internal::ErrorUtils::StackPropertyLookupResult lookup =
      v8::internal::ErrorUtils::GetErrorStackProperty(isolate, error);
  if (v8::internal::IsErrorStackData(*lookup.error_stack)) {
    return v8::internal::Cast<v8::internal::ErrorStackData>(lookup.error_stack)
        ->HasFormattedStack();
  }
  return v8::internal::IsString(*lookup.error_stack);
}

}  // namespace replayio
}  // namespace v8
