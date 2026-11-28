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


}  // namespace

namespace {

// Whether getting |name| on |receiver| reads a plain string: a data property up
// the chain, or nothing at all, with no accessor, proxy, interceptor or access
// check on the way and no value to convert.
bool ReadsPlainString(
    v8::internal::Isolate* isolate,
    v8::internal::DirectHandle<v8::internal::JSReceiver> receiver,
    v8::internal::DirectHandle<v8::internal::Name> name) {
  v8::internal::LookupIterator it(isolate, receiver, name);
  if (!it.IsFound()) {
    return true;
  }
  if (it.state() != v8::internal::LookupIterator::DATA) {
    return false;
  }
  return v8::internal::IsString(*it.GetDataValue());
}

// Whether reading an Error's own "stack" runs the page's code. The getter
// formats the stack where none is stored yet, and formatting reaches the page
// through its Error.prepareStackTrace, the lookup of that property on the Error
// constructor, and the name and message V8's own formatter reads. A stack
// already stored costs none of that, since the getter hands that string back.
bool ErrorStackReadRunsUserJS(
    v8::internal::Isolate* isolate,
    v8::internal::DirectHandle<v8::internal::JSObject> error) {
  v8::internal::ErrorUtils::StackPropertyLookupResult lookup =
      v8::internal::ErrorUtils::GetErrorStackProperty(isolate, error);
  if (v8::internal::IsErrorStackData(*lookup.error_stack)) {
    if (v8::internal::Cast<v8::internal::ErrorStackData>(lookup.error_stack)
            ->HasFormattedStack()) {
      return false;
    }
  } else if (!v8::internal::IsFixedArray(*lookup.error_stack)) {
    return false;
  }

  // An embedder callback formats in place of all of this, and what it runs is
  // its own business. Chromium installs none, node does.
  if (isolate->HasPrepareStackTraceCallback()) {
    return true;
  }
  v8::internal::DirectHandle<v8::internal::NativeContext> error_context;
  if (!error->GetCreationContext(isolate).ToHandle(&error_context)) {
    return true;
  }
  v8::internal::DirectHandle<v8::internal::JSFunction> error_function(
      error_context->error_function(), isolate);
  v8::internal::LookupIterator it(
      isolate, error_function,
      isolate->factory()->InternalizeUtf8String("prepareStackTrace"));
  if (it.IsFound()) {
    if (it.state() != v8::internal::LookupIterator::DATA) {
      return true;
    }
    if (v8::internal::IsJSFunction(*it.GetDataValue())) {
      return true;
    }
  }

  // Without one, V8 builds the stack out of the frames itself, reading the
  // Error's name and message the way Error.prototype.toString does.
  return !ReadsPlainString(isolate, error, isolate->factory()->name_string()) ||
         !ReadsPlainString(isolate, error,
                           isolate->factory()->message_string());
}

}  // namespace

bool ReadingIsFreeOfUserJS(v8::Local<v8::Object> object,
                           v8::Local<v8::Name> name) {
  v8::internal::Isolate* isolate = v8::internal::Isolate::Current();
  v8::internal::DirectHandle<v8::internal::JSReceiver> receiver =
      Utils::OpenDirectHandle(*object);
  if (!v8::internal::IsJSObject(*receiver)) {
    return false;
  }

  // One of V8's own getters, as an own accessor of the object, so that reading
  // |name| really calls it: a page getter shadowing an inherited one, a proxy
  // trap, an interceptor and an access check all end the lookup elsewhere.
  v8::internal::PropertyKey key(isolate, Utils::OpenDirectHandle(*name));
  if (key.is_element()) {
    return false;
  }
  v8::internal::LookupIterator it(isolate, receiver, key,
                                  v8::internal::LookupIterator::OWN);
  if (!it.IsFound() || it.state() != v8::internal::LookupIterator::ACCESSOR) {
    return false;
  }
  v8::internal::DirectHandle<v8::internal::Object> accessors =
      it.GetAccessors();
  if (!v8::internal::IsAccessorPair(*accessors)) {
    return false;
  }
  v8::internal::Tagged<v8::internal::Object> getter =
      v8::internal::Cast<v8::internal::AccessorPair>(*accessors)->getter();
  if (!v8::internal::IsJSFunction(getter)) {
    return false;
  }
  v8::internal::Tagged<v8::internal::SharedFunctionInfo> shared =
      v8::internal::Cast<v8::internal::JSFunction>(getter)->shared();
  if (!shared->IsApiFunction()) {
    return false;
  }

  // Each getter needs its own answer, since what a read costs depends on what
  // it does with the state the receiver is in, and the one V8 marks as having
  // an effect today is an Error's stack. What would generalize this is the
  // side-effect type V8 declares on the template a getter is an API function
  // of (FunctionTemplateInfo::has_side_effects), but reading every getter V8
  // declares free of them is a wider change than this gate: it would report
  // values for builtin getters the gate refuses today, with no case covering
  // any of them.
  if (shared->api_func_data() !=
      *isolate->factory()->error_stack_getter_fun_template()) {
    return false;
  }
  return !ErrorStackReadRunsUserJS(
      isolate, v8::internal::Cast<v8::internal::JSObject>(receiver));
}

bool IsEmbedderGetterInPause(v8::Local<v8::Function> getter) {
  if (!v8::recordreplay::HasDivergedFromRecording()) {
    return false;
  }
  v8::internal::DirectHandle<v8::internal::JSReceiver> function =
      Utils::OpenDirectHandle(*getter);
  if (!v8::internal::IsJSFunction(*function)) {
    return false;
  }
  v8::internal::Tagged<v8::internal::SharedFunctionInfo> shared =
      v8::internal::Cast<v8::internal::JSFunction>(*function)->shared();
  return shared->IsApiFunction() &&
         !shared->api_func_data()->has_side_effects();
}

}  // namespace replayio
}  // namespace v8
