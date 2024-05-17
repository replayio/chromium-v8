#include "include/replayio.h"
#include "src/replayio/replayio-util.h"
#include "src/execution/isolate-inl.h"

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
  Isolate* isolate = (v8::Isolate*)isolate;
  Handle<Context> ctx = context.Get(isolate);
  Local<String> propNameLocal = CStringToLocal(isolate, propName.c_str());
  Local<Object> callbackValue = object->Get(ctx, propNameLocal).ToLocalChecked();
  CHECK(callbackValue->IsFunction());

  // Ensure that the function's context still exists.
  callbackValue->GetCreationContextChecked();

  return callbackValue.As<v8::Function>();
}

bool CallGlobalFunctionUnchecked(Local<v8::Function> callback,
                                 i::Handle<i::Object> paramsObj) {
  
}

i::Handle<i::Object> ReplayRootContext::CallGlobalFunction(const std::string& functionName,
                                                           int argc,
                                                           i::Handle<i::Object> argv[]) {
  i::Isolate* i_isolate = i::Isolate::Current();
  Isolate* isolate = (v8::Isolate*)isolate;
  Local<Context> ctx = context.Get(isolate);

  // Get the callback.
  Local<Function> fn = GetFunction(ctx->Global(), functionName);
  
  // Make the call.
  i::Handle<i::Object> fnHandle = Utils::OpenHandle(*fn);
  i::Handle<i::Object> undefined = i_isolate->factory()->undefined_value();
  v8::TryCatch try_catch((v8::Isolate*)isolate);
  i::MaybeHandle<i::Object> rv =
      i::Execution::Call(i_isolate, fnHandle, undefined, argc, argv);

  // Handle result.
  if (try_catch.HasCaught()) {
    Local<Message> msg = try_catch.Message();
    v8::String::Utf8Value msgString((v8::Isolate*)isolate, msg->Get());
    recordreplay::Crash("CallGlobalFunction(%s) failed (%d:%d): %s",
                        functionName.c_str(),
                        msg->GetLineNumber(ctx).FromMaybe(-1),
                        msg->GetStartColumn(ctx).FromJust(), *msgString);
  }
  if (rv.is_null()) {
    recordreplay::Crash("CallGlobalFunction(%s) failed without error", functionName.c_str());
  }
  return rv.ToHandleChecked();
}

i::Handle<i::Object> ReplayRootContext::CallRegisteredCallback(const std::string& callbackName,
                                                               i::Handle<i::Object> paramsObj) {
  i::Isolate* i_isolate = i::Isolate::Current();
  Isolate* isolate = (v8::Isolate*)isolate;
  Local<Context> ctx = context.Get(isolate);

  // Prepare args.
  constexpr int NCallArgs = 3;
  i::Handle<i::Object> callArgs[NCallArgs];
  callArgs[0] = callbackRegistry;
  callArgs[1] = CStringToHandle(i_isolate, callbackName.c_str());
  callArgs[2] = paramsObj;
  
  return CallGlobalFunction(TODO);
}

ReplayRootContext* RecordReplayCreateRootContext(v8::Isolate* isolate, v8::Local<v8::Context> cx) {
  CHECK(IsMainThread());
  if (gReplayRootContext) {
    delete gReplayRootContext;
  }
  gReplayRootContext = new ReplayRootContext();
  gReplayRootContext->context = Eternal<v8::Context>(isolate, cx);
}

extern "C" void V8RecordReplayGetDefaultContext(v8::Isolate* isolate, v8::Local<v8::Context>* cx) {
  CHECK(IsMainThread() && gReplayRootContext);
  *cx = gReplayRootContext->context.Get(isolate);
}

bool RecordReplayHasDefaultContext() {
  return !!gReplayRootContext;
}

}  // namespace replayio
}  // namespace v8
