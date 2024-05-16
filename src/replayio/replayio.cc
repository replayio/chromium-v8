#include "src/replayio/replayio.h"

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

bool ReplayRootContext::CallCallback(std::string&& callbackName,
                                     Handle<Object> paramsObj) {
  // Get the callback.
  i::Isolate* isolate = Isolate::Current();
  Local<String> nameV8 = toV8String(isolate, callbackName.c_str());
  Local<Value> callbackFunction;
  if (!context->Global()->Get(info.context, nameV8).ToLocal(&callbackFunction) ||
      !callbackFunction->IsFunction()) {
    return false;
  }
  Local<v8::Function> callbackValue =
      callbackFunction->Get((v8::Isolate*)isolate);
  Handle<Object> callbackHandle = Utils::OpenHandle(*callbackValue);
  Handle<Object> undefined = isolate->factory()->undefined_value();

  // Prepare args.
  constexpr int NCallArgs = 3;
  Handle<Object> callArgs[NCallArgs];
  callArgs[0] = callbackRegistry;
  callArgs[1] = CStringToHandle(isolate, callbackName.c_str());
  callArgs[2] = paramsObj;

  // Make the call.
  v8::TryCatch try_catch((v8::Isolate*)isolate);
  MaybeHandle<Object> rv =
      Execution::Call(isolate, callbackHandle, undefined, NCallArgs, callArgs);

  // Handle result.
  if (try_catch.HasCaught()) {
    Local<v8::Context> context = Utils::ToLocal(context_handle);
    Local<Message> msg = try_catch.Message();
    v8::String::Utf8Value msgString((v8::Isolate*)isolate, msg->Get());
    recordreplay::Crash("CallCallback(%s) failed (%d:%d): %s",
                        callbackName.c_str(),
                        msg->GetLineNumber(context).FromMaybe(-1),
                        msg->GetStartColumn(context).FromJust(), *msgString);
  }
  if (rv.is_null()) {
    recordreplay::Crash("CallCallback(%s) failed", callbackName.c_str());
  }
}

/** ###########################################################################
 * Utilities.
 * ##########################################################################*/

void CHECKIsJSFunction(v8::Isolate* isolate, Local<Value> value) {
  HandleScope scope(isolate);
  i::Handle<i::Object> value_handle =
      Utils::OpenHandle(*value.As<v8::Object>());
  // i::Handle<i::JSFunction> function =
  // i::Handle<i::JSFunction>::cast(handler); i::Handle<i::Context>
  // context_handle = i::Handle<i::Context>(
  //   function->context().script_context(),
  //   isolate
  // );
  CHECK(value_handle->IsJSFunction());
}

}  // namespace replayio
}  // namespace v8
