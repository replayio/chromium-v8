#include "src/api/api-inl.h"
#include "src/base/replayio.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/messages.h"
#include "src/heap/factory.h"
#include "src/objects/api-callbacks-inl.h"
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

// Whether reading an Error's own "stack" runs the page's code. The getter
// formats the stack where none is stored yet, and formatting reaches the page
// through its Error.prepareStackTrace, the lookup of that property on the Error
// constructor, and the name and message V8's own formatter reads. A stack
// already stored costs none of that, since the getter hands that string back.
bool ErrorStackReadRunsUserJS(
    v8::internal::Isolate* isolate,
    v8::internal::Handle<v8::internal::JSObject> error) {
  v8::internal::Handle<v8::internal::Object> error_stack =
      v8::internal::JSReceiver::GetDataProperty(
          isolate, error, isolate->factory()->error_stack_symbol());
  if (error_stack->IsErrorStackData()) {
    if (v8::internal::Handle<v8::internal::ErrorStackData>::cast(error_stack)
            ->HasFormattedStack()) {
      return false;
    }
  } else if (!error_stack->IsFixedArray()) {
    return false;
  }

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

bool ReadingRunsUserJS(v8::Local<v8::Object> object,
                       v8::Local<v8::Name> name) {
  v8::internal::Isolate* isolate = v8::internal::Isolate::Current();
  v8::internal::Handle<v8::internal::JSReceiver> receiver =
      Utils::OpenHandle(*object);

  // One of V8's own accessors, as an own property of the object. That is the
  // whole category: DebugPropertyIterator reports V8's own accessors as
  // ordinary properties rather than native ones, so a description takes a
  // descriptor for them, and taking the descriptor of an AccessorInfo reads it
  // by calling the getter. An accessor the page installed is not here, since
  // the description leaves a page getter alone already.
  v8::internal::PropertyKey key(isolate, Utils::OpenHandle(*name));
  if (key.is_element()) {
    return false;
  }
  v8::internal::LookupIterator it(isolate, receiver, key,
                                  v8::internal::LookupIterator::OWN);
  if (!it.IsFound() || it.state() != v8::internal::LookupIterator::ACCESSOR) {
    return false;
  }
  v8::internal::Handle<v8::internal::Object> structure = it.GetAccessors();
  if (!structure->IsAccessorInfo()) {
    return false;
  }
  v8::internal::Handle<v8::internal::AccessorInfo> accessor =
      v8::internal::Handle<v8::internal::AccessorInfo>::cast(structure);

  // V8 says which of its accessors a read can have an effect through, and the
  // rest cost nothing to read: in ACCESSOR_INFO_LIST_GENERATOR every getter is
  // kHasNoSideEffect but error_stack's, so array_length, function_name,
  // function_prototype and the others all end here.
  if (accessor->getter_side_effect_type() ==
      v8::SideEffectType::kHasNoSideEffect) {
    return false;
  }

  // What a read costs past that gate is particular to the accessor, so each one
  // V8 marks this way needs its own answer. V8's own general answer is no help:
  // Debug::PerformSideEffectCheck goes by the declared type and allows
  // kHasSideEffectToReceiver only for an object created during the evaluation,
  // so it refuses to read a page's Error whatever state it is in, and the fork
  // short-circuits that check altogether while replaying with events
  // disallowed.
  if (*accessor == *isolate->factory()->error_stack_accessor()) {
    return receiver->IsJSObject() &&
           ErrorStackReadRunsUserJS(
               isolate,
               v8::internal::Handle<v8::internal::JSObject>::cast(receiver));
  }

  // An accessor V8 marks as having an effect with no answer here. Reading it is
  // assumed to reach the page.
  return true;
}

}  // namespace replayio
}  // namespace v8
