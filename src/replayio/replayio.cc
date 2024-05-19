#include "include/replayio.h"
#include "src/replayio/replayio-util.h"
#include "src/execution/isolate-inl.h"
#include "src/handles/handles.h"

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

v8::Local<v8::Function> ReplayRootContext::GetFunction(
    v8::Local<v8::Object> object,
    const std::string& propName
  ) {
  CHECK(IsMainThread());
  i::Isolate* i_isolate = i::Isolate::Current();
  Isolate* isolate = (v8::Isolate*)i_isolate;
  Handle<Context> ctx = context_.Get(isolate);
  Local<String> propNameLocal = CStringToLocal(isolate, propName.c_str());
  Local<Object> callbackValue = object->Get(ctx, propNameLocal).ToLocalChecked();
  CHECK(callbackValue->IsFunction());

  // Ensure that the function's context still exists.
  callbackValue->GetCreationContextChecked();

  return callbackValue.As<v8::Function>();
}

Local<Value> ReplayRootContext::CallFunction(Local<v8::Function> fn,
                                             int argc,
                                             Local<Value> argv[]) {
  i::Isolate* i_isolate = i::Isolate::Current();
  Isolate* isolate = (v8::Isolate*)i_isolate;
  Local<Context> ctx = context_.Get(isolate);

  // Make the call.
  Local<Value> undefined = Undefined(isolate);
  v8::TryCatch try_catch(isolate);
  MaybeLocal<v8::Value> rv = fn->Call(ctx, undefined, argc, argv);

  // Handle result.
  if (try_catch.HasCaught()) {
    Local<Message> msg = try_catch.Message();
    v8::String::Utf8Value functionName(isolate, fn->GetName());
    v8::String::Utf8Value msgString(isolate, msg->Get());
    recordreplay::Crash("CallFunction(%s) failed (%d:%d): %s",
                        functionName.length() ? *functionName : "",
                        msg->GetLineNumber(ctx).FromMaybe(-1),
                        msg->GetStartColumn(ctx).FromJust(),
                        msgString.length() ? *msgString : "");
  }
  if (rv.IsEmpty()) {
    recordreplay::Crash("CallFunction(%s) failed without error", functionName.c_str());
  }
  return rv.ToLocalChecked();
}

Local<Value> ReplayRootContext::CallGlobalFunction(const std::string& functionName,
                                                   int argc,
                                                   Local<Value> argv[]) {
  i::Isolate* i_isolate = i::Isolate::Current();
  Isolate* isolate = (v8::Isolate*)i_isolate;
  Local<Context> ctx = context_.Get(isolate);

  // Get the callback.
  Local<Function> fn = GetFunction(ctx->Global(), functionName);
  return CallFunction(fn, argc, argv);
}

Local<Value> ReplayRootContext::CallRegisteredCallback(const std::string& callbackName,
                                                       Local<Object> param1) {
  i::Isolate* i_isolate = i::Isolate::Current();
  Isolate* isolate = (v8::Isolate*)i_isolate;
  Local<Context> ctx = context_.Get(isolate);

  // Prepare args.
  constexpr int NCallArgs = 3;
  Local<Value> callArgs[NCallArgs];
  callArgs[0] = callbackRegistry_.Get(isolate);
  callArgs[1] = CStringToLocal(isolate, callbackName.c_str());
  callArgs[2] = param1;
  
  return CallGlobalFunction(std::string(FunctionCallRegisteredCallback), NCallArgs, callArgs);
}

ReplayRootContext* RecordReplayCreateRootContext(v8::Isolate* isolate, v8::Local<v8::Context> cx) {
  CHECK(IsMainThread());
  if (gReplayRootContext) {
    delete gReplayRootContext;
  }
  gReplayRootContext = new ReplayRootContext();
  gReplayRootContext->context_ = Eternal<v8::Context>(isolate, cx);
}

extern "C" void V8RecordReplayGetDefaultContext(v8::Isolate* isolate, v8::Local<v8::Context>* cx) {
  CHECK(IsMainThread() && gReplayRootContext);
  *cx = gReplayRootContext->context_.Get(isolate);
}

bool RecordReplayHasDefaultContext() {
  return !!gReplayRootContext;
}

}  // namespace replayio
}  // namespace v8
