#include "src/replayio/replayio-util.h"

#include "src/api/api.h"

namespace v8 {
namespace i = internal;
namespace replayio {



i::Handle<i::String> CStringToHandle(i::Isolate* isolate, const char* str) {
  return isolate->factory()->NewStringFromUtf8(base::CStrVector(str)).ToHandleChecked();
}


Local<String> CStringToLocal(Isolate* isolate, const char* str) {
  return v8::String::NewFromUtf8(isolate, str,
                                 v8::NewStringType::kInternalized).ToLocalChecked();
}

i::Handle<i::Object> GetProperty(i::Isolate* isolate,
                                 i::Handle<i::Object> obj, const char* property) {
  return i::Object::GetProperty(isolate, obj, CStringToHandle(isolate, property))
    .ToHandleChecked();
}

void SetProperty(i::Isolate* isolate,
                 i::Handle<i::Object> obj, const char* property,
                 i::Handle<i::Object> value) {
  i::Object::SetProperty(isolate, obj,
                         CStringToHandle(isolate, property), value).Check();
}

void SetProperty(i::Isolate* isolate,
                 i::Handle<i::Object> obj, const char* property,
                 const char* value) {
  SetProperty(isolate, obj, property, CStringToHandle(isolate, value));
}

void SetProperty(i::Isolate* isolate,
                        i::Handle<i::Object> obj, const char* property,
                        double value) {
  SetProperty(isolate, obj, property, isolate->factory()->NewNumber(value));
}

i::Handle<i::JSObject> NewPlainObject(i::Isolate* isolate) {
  return isolate->factory()->NewJSObject(isolate->object_function());
}

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
