#include "src/api/api-inl.h"
#include "src/base/replayio.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/messages.h"
#include "src/heap/factory.h"
#include "src/objects/lookup.h"
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

namespace {

// Whether getting |name| on |receiver| reads a plain string: a data property
// up the chain, or nothing at all, with no accessor, proxy, interceptor or
// access check on the way and no value to convert.
bool ReadsPlainString(v8::internal::Isolate* isolate,
                      v8::internal::Handle<v8::internal::JSReceiver> receiver,
                      v8::internal::Handle<v8::internal::Name> name) {
  v8::internal::LookupIterator it(isolate, receiver, name);
  if (!it.IsFound()) {
    return true;
  }
  if (it.state() != v8::internal::LookupIterator::DATA) {
    return false;
  }
  return it.GetDataValue()->IsString();
}

// Whether formatting this Error's stack runs the page's code: a
// prepareStackTrace function for V8 to call, or the page standing in the way
// of the reads V8's own formatter makes.
bool FormattingRunsUserJS(
    v8::internal::Isolate* isolate,
    v8::internal::Handle<v8::internal::JSObject> error) {
  // An embedder callback formats in place of all of this, and what it runs is
  // its own business. Chromium installs none, node does.
  if (isolate->HasPrepareStackTraceCallback()) {
    return true;
  }
  v8::internal::Handle<v8::internal::Context> error_context;
  if (!error->GetCreationContext().ToHandle(&error_context)) {
    return true;
  }
  v8::internal::Handle<v8::internal::JSFunction> error_function(
      error_context->error_function(), isolate);
  v8::internal::LookupIterator it(
      isolate, error_function,
      isolate->factory()->InternalizeUtf8String("prepareStackTrace"));
  if (it.IsFound()) {
    if (it.state() != v8::internal::LookupIterator::DATA) {
      return true;
    }
    if (it.GetDataValue()->IsJSFunction()) {
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

bool MustNotFormatErrorStack(v8::Local<v8::Object> object,
                             v8::Local<v8::Name> name) {
  if (!MustNotRunUserJS()) {
    return false;
  }
  v8::internal::Isolate* isolate = v8::internal::Isolate::Current();

  // Only an Error's own "stack". Checking the name first keeps the lookups
  // below off every other property the inspector walks.
  if (*Utils::OpenHandle(*name) != *isolate->factory()->stack_string()) {
    return false;
  }
  v8::internal::Handle<v8::internal::JSReceiver> receiver =
      Utils::OpenHandle(*object);
  if (!receiver->IsJSObject()) {
    return false;
  }
  v8::internal::Handle<v8::internal::JSObject> error =
      v8::internal::Handle<v8::internal::JSObject>::cast(receiver);

  // V8's own Error stack accessor, as an own property, so that reading "stack"
  // on |object| really calls it: a page getter shadowing an inherited one, a
  // proxy trap, an interceptor and an access check all end the lookup
  // elsewhere.
  v8::internal::LookupIterator it(isolate, error,
                                  isolate->factory()->stack_string(), error,
                                  v8::internal::LookupIterator::OWN);
  if (it.state() != v8::internal::LookupIterator::ACCESSOR) {
    return false;
  }
  v8::internal::Handle<v8::internal::Object> accessors = it.GetAccessors();
  if (*accessors != *isolate->factory()->error_stack_accessor()) {
    return false;
  }

  // The shapes ErrorUtils::GetFormattedStack formats, which is what reaches
  // the page: its Error.prepareStackTrace, the lookup of that property on the
  // Error constructor, and the name and message it reads when there is none. A
  // stack already stored costs none of that, since the getter hands the stored
  // string back.
  v8::internal::Handle<v8::internal::Object> error_stack =
      v8::internal::JSReceiver::GetDataProperty(
          isolate, error, isolate->factory()->error_stack_symbol());
  bool formats;
  if (error_stack->IsErrorStackData()) {
    formats = !v8::internal::Handle<v8::internal::ErrorStackData>::cast(
                   error_stack)
                   ->HasFormattedStack();
  } else {
    formats = error_stack->IsFixedArray();
  }
  if (!formats) {
    return false;
  }

  // Formatting is only off limits while the page's code would run as part of
  // it. Where it would not, the stack V8 builds is the string the page's own
  // read would get, so the property reports it as it always has.
  return FormattingRunsUserJS(isolate, error);
}

}  // namespace replayio
}  // namespace v8
