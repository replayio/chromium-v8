#include "src/base/replayio.h"
#include "src/handles/global-handles-inl.h"

namespace v8 {
namespace i = internal;
namespace replayio {

void CHECKIsJSFunction(v8::Isolate* isolate, Local<Value> value) {
  HandleScope scope(isolate);
  i::Handle<i::Object> value_handle = Utils::OpenHandle(*value.As<v8::Object>());
  // i::Handle<i::JSFunction> function = i::Handle<i::JSFunction>::cast(handler);
  // i::Handle<i::Context> context_handle = i::Handle<i::Context>(
  //   function->context().script_context(),
  //   isolate
  // );
  CHECK(value_handle->IsJSFunction());
}

}  // namespace replayio
}  // namespace v8
