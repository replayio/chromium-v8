#include "include/replayio.h"
#include "src/replayio/replayio-util.h"
#include "src/execution/isolate-inl.h"
#include "src/handles/handles.h"
#include "src/api/api-inl.h"

namespace v8 {
namespace i = internal;
namespace replayio {


/** ###########################################################################
 * Globals.
 * ##########################################################################*/

v8::replayio::ReplayRootContext* gReplayRootContext;

/** ###########################################################################
 * ReplayRootContext
 * ##########################################################################*/

Local<v8::Context> ReplayRootContext::GetContext() const {
  i::Isolate* i_isolate = i::Isolate::Current();
  Isolate* isolate = (v8::Isolate*)i_isolate;
  return context_.Get(isolate);
}

Local<Object> ReplayRootContext::GetEventEmitter() const {
  i::Isolate* i_isolate = i::Isolate::Current();
  Isolate* isolate = (v8::Isolate*)i_isolate;
  return eventEmitter_.Get(isolate);
}

v8::Local<v8::Function> ReplayRootContext::GetFunction(
    v8::Local<v8::Object> object,
    const std::string& propName
  ) const {
  CHECK(IsMainThread());
  i::Isolate* i_isolate = i::Isolate::Current();
  Isolate* isolate = (v8::Isolate*)i_isolate;
  Local<v8::Context> cx = GetContext();
  Local<String> propNameLocal = CStringToLocal(isolate, propName.c_str());
  Local<Value> callbackValue;
  if (!object->Get(cx, propNameLocal).ToLocal(&callbackValue)) {
    recordreplay::Crash(
      "ReplayRootContext::GetFunction: NO_FUNCTION - Function does not exist: %s",
      propName.c_str()
    );
  }
  CHECK(callbackValue->IsFunction());

  // Sanity check: The function's creation context should still exist.
  callbackValue.As<Function>()->GetCreationContextChecked();

  return callbackValue.As<v8::Function>();
}

Local<Value> ReplayRootContext::CallFunction(Local<v8::Function> fn,
                                             int argc,
                                             Local<Value> argv[],
                                             MaybeLocal<Value> maybeReceiver) const {
  i::Isolate* i_isolate = i::Isolate::Current();
  Isolate* isolate = (v8::Isolate*)i_isolate;
  Local<v8::Context> cx = GetContext();

  // Make the call.
  Local<Value> undefined = Undefined(isolate);
  Local<Value> receiver;
  if (!maybeReceiver.ToLocal(&receiver)) {
    receiver = undefined;
  }
  v8::TryCatch try_catch(isolate);
  MaybeLocal<v8::Value> rv = fn->Call(cx, receiver, argc, argv);

  // Handle result.
  if (try_catch.HasCaught()) {
    Local<Message> msg = try_catch.Message();
    v8::String::Utf8Value functionName(isolate, fn->GetName());
    v8::String::Utf8Value msgString(isolate, msg->Get());
    recordreplay::Crash("CallFunction(%s) failed (%d:%d): %s",
                        functionName.length() ? *functionName : "",
                        msg->GetLineNumber(cx).FromMaybe(-1),
                        msg->GetStartColumn(cx).FromJust(),
                        msgString.length() ? *msgString : "");
  }
  if (rv.IsEmpty()) {
    v8::String::Utf8Value functionName(isolate, fn->GetName());
    recordreplay::Crash("CallFunction(%s) failed without error",
      functionName.length() ? *functionName : "");
  }
  return rv.ToLocalChecked();
}

Local<Value> ReplayRootContext::CallGlobalFunction(const std::string& functionName,
                                                   int argc,
                                                   Local<Value> argv[]) const {
  i::Isolate* i_isolate = i::Isolate::Current();
  // Isolate* isolate = (v8::Isolate*)i_isolate;
  Local<v8::Context> cx = GetContext();

  // Get the callback.
  Local<Function> fn = GetFunction(cx->Global(), functionName);
  return CallFunction(fn, argc, argv);
}

Local<Value> ReplayRootContext::EmitReplayEvent(const std::string& eventName,
                                                MaybeLocal<Object> param1) const {
  i::Isolate* i_isolate = i::Isolate::Current();
  Isolate* isolate = (v8::Isolate*)i_isolate;
  // Local<v8::Context> cx = GetContext();

  Local<Function> fn = GetFunction(GetEventEmitter(), eventName);

  // Prepare args.
  constexpr int NCallArgs = 1;
  Local<Value> callArgs[NCallArgs] = {
    param1.IsEmpty() ? Undefined(isolate).As<Value>() : param1.ToLocalChecked().As<Value>()
  };
  
  return CallFunction(fn, NCallArgs, callArgs, GetEventEmitter());
}

ReplayRootContext* RecordReplayCreateRootContext(v8::Isolate* isolate, v8::Local<v8::Context> cx) {
  CHECK(IsMainThread());
  if (gReplayRootContext) {
    delete gReplayRootContext;
    // gReplayRootContext = nullptr;
  }
  return gReplayRootContext = new ReplayRootContext(Eternal<v8::Context>(isolate, cx));
}

ReplayRootContext* RecordReplayGetRootContext(v8::Context cx) {
  CHECK(IsMainThread());
  // TODO: Implement this.
  return gReplayRootContext;
}

ReplayRootContext* RecordReplayGetRootContext() {
  CHECK(IsMainThread());
  return gReplayRootContext;
}

/**
 * @deprecated There is no single "default context".
 */
extern "C" void V8RecordReplayGetDefaultContext(v8::Isolate* isolate, v8::Local<v8::Context>* cx) {
  ReplayRootContext* root = RecordReplayGetRootContext();
  CHECK(IsMainThread() && root);
  *cx = root->GetContext();
}

bool RecordReplayHasDefaultContext() {
  return !!gReplayRootContext;
}

}  // namespace replayio
}  // namespace v8
