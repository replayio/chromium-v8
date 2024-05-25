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
  if (!callbackValue->IsFunction()) {
    recordreplay::Crash(
      "ReplayRootContext::GetFunction: NOT_A_FUNCTION - Emitter prop is not function: %s",
      propName.c_str()
    );
  }

  // Sanity check: The function's creation context should still exist.
  callbackValue.As<Function>()->GetCreationContextChecked();

  return callbackValue.As<v8::Function>();
}

template<class T>
static Local<T> CrashOnError(const char* task, const char* target_name, Local<v8::Context> cx, const v8::TryCatch& try_catch, MaybeLocal<T> rv) {
  Isolate* isolate = v8::Isolate::GetCurrent();
  if (try_catch.HasCaught()) {
    Local<Message> msg = try_catch.Message();
    v8::String::Utf8Value msgString(isolate, msg->Get());
    recordreplay::Crash("%s(%s) failed (%d:%d): %s",
                        task,
                        target_name,
                        msg->GetLineNumber(cx).FromMaybe(-1),
                        msg->GetStartColumn(cx).FromJust(),
                        msgString.length() ? *msgString : "");
  }

  Local<T> rv_real;
  if (!rv.ToLocal(&rv_real)) {
    recordreplay::Crash("%s(%s) failed without error",
      task,
      target_name);
  }
  return rv_real;
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
  
  // Sanity check: The function's creation context should still exist.
  fn.As<Function>()->GetCreationContextChecked();
  
  v8::TryCatch try_catch(isolate);
  MaybeLocal<v8::Value> rv = fn->Call(cx, receiver, argc, argv);

  v8::String::Utf8Value functionName(isolate, fn->GetName());
  const char* targetName = functionName.length() ? *functionName : "";
  return CrashOnError("CallFunction", targetName, cx, try_catch, rv);
}

Local<Value> ReplayRootContext::CallGlobalFunction(const std::string& functionName,
                                                   int argc,
                                                   Local<Value> argv[]) const {
  // i::Isolate* i_isolate = i::Isolate::Current();
  // Isolate* isolate = (v8::Isolate*)i_isolate;
  Local<v8::Context> cx = GetContext();

  // Get the callback.
  Local<Function> fn = GetFunction(cx->Global(), functionName);
  return CallFunction(fn, argc, argv);
}

Local<Value> ReplayRootContext::EmitReplayEvent(const std::string& eventName,
                                                Local<Value> param1,
                                                const std::string& emitName) const {
  // i::Isolate* i_isolate = i::Isolate::Current();
  // Isolate* isolate = (v8::Isolate*)i_isolate;

  // Prepare args.
  constexpr int NCallArgs = 1;
  Local<Value> callArgs[NCallArgs] = { param1 };
  
  return EmitReplayEvent(eventName, NCallArgs, callArgs, emitName);
}

Local<Value> ReplayRootContext::EmitReplayEvent(const std::string& eventName,
                                                int eventArgc,
                                                Local<Value> eventArgv[],
                                                const std::string& emitName) const {
  i::Isolate* i_isolate = i::Isolate::Current();
  Isolate* isolate = (v8::Isolate*)i_isolate;

  Local<Function> fn = GetFunction(GetEventEmitter(), emitName);

  // Inject the event name as first parameter.
  const int argc = eventArgc + 1;
  Local<Value>* argv = new Local<Value>[argc];
  argv[0] = CStringToLocal(isolate, eventName.c_str());
  std::copy(eventArgv, eventArgv + eventArgc, argv + 1);
  
  return CallFunction(fn, argc, argv, GetEventEmitter());
}

v8::Local<v8::Value> ReplayRootContext::RunScriptAndCallBack(
    const std::string& souce_raw, const std::string& filename
  ) {
  Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::String> filename_string = CStringToLocal(isolate, filename.c_str());
  v8::ScriptOrigin origin(isolate, filename_string);

  v8::TryCatch try_catch(isolate);
  CHECK(souce_raw.length());
  v8::Local<v8::String> source = CStringToLocal(isolate, souce_raw.c_str());
  v8::Local<v8::Context> cx = GetContext();

  auto maybe_script = v8::Script::Compile(cx, source, &origin);
  CrashOnError("RunScriptAndCallBack", filename.c_str(), cx, try_catch, maybe_script);

  v8::MaybeLocal<v8::Value> maybe_rv = maybe_script.ToLocalChecked()->Run(cx);
  Local<v8::Value> rv = CrashOnError("RunScriptAndCallBack", filename.c_str(), cx, try_catch, maybe_rv);

  recordreplay::Print("DDBG RunScriptAndCallback %d", !!rv->IsFunction());
  if (rv->IsFunction()) {
    constexpr int Argc = 1;
    v8::Local<v8::Value> argv[] = {
      GetEventEmitter()
    };
    CallFunction(rv.As<v8::Function>(), Argc, argv);
  }
  return rv;
}

ReplayRootContext* RecordReplayCreateRootContext(v8::Isolate* isolate, v8::Local<v8::Context> cx) {
  CHECK(IsMainThread());
  if (gReplayRootContext) {
    delete gReplayRootContext;
    // gReplayRootContext = nullptr;
  }
  Local<v8::Object> eventEmitter = v8::Object::New(isolate);
  return gReplayRootContext = new ReplayRootContext(
    Eternal<v8::Context>(isolate, cx),
    Eternal<v8::Object>(isolate, eventEmitter)
  );
}

ReplayRootContext* RecordReplayGetRootContext(v8::Local<v8::Context> cx) {
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

bool RecordReplayIsReplayJsCode(const char* url) {
  return !strncmp(url, "record-replay-internal://", 25);
}

}  // namespace replayio
}  // namespace v8
