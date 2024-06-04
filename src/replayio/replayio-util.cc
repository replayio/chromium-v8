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

std::unique_ptr<char[]> LocalToCString(Isolate* isolate, Local<String> str) {
  v8::String::Utf8Value text(isolate, str);
  return std::unique_ptr<char[]>(*text);
}

std::unique_ptr<char[]> HandleToCString(i::Handle<i::String> str) {
  return i::String::cast(*str).ToCString();
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

void CrashOnError(const char* task, const char* target_name, Local<v8::Context> cx, const v8::TryCatch& try_catch) {
  Isolate* isolate = v8::Isolate::GetCurrent();
  if (try_catch.HasCaught()) {
    Local<Message> msg = try_catch.Message();
    Local<Value> exception_obj = v8::Exception::Error(msg->Get());
    v8::String::Utf8Value msg_source_string(isolate, msg->GetScriptResourceName());
    Local<Value> local_stack_trace_strinxg;
    std::string error_detail;
    if (v8::TryCatch::StackTrace(cx, exception_obj)
          .ToLocal(&local_stack_trace_strinxg) &&
        local_stack_trace_strinxg->IsString()) {
      v8::String::Utf8Value stack_trace(isolate, local_stack_trace_strinxg.As<String>());
      error_detail = *stack_trace ? *stack_trace : "";
    }
    if (!error_detail.length()) {
      v8::String::Utf8Value exception_string(isolate, exception_obj);
      error_detail = *exception_string ? *exception_string : "";
    }
    recordreplay::Crash("%s(%s) failed (at %s:%d:%d) - %s",
                        task,
                        target_name,
                        *msg_source_string ? *msg_source_string : "",
                        msg->GetLineNumber(cx).FromMaybe(-1),
                        msg->GetStartColumn(cx).FromJust(),
                        // stack.c_str()
                        error_detail.c_str()
                        );
  }
}

}  // namespace replayio
}  // namespace v8
