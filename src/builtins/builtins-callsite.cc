// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/heap/heap-inl.h"  // For ToBoolean.
#include "src/logging/counters.h"
#include "src/objects/call-site-info-inl.h"
#include "src/objects/objects-inl.h"
#include "src/replay/replayio.h"

namespace v8 {
namespace internal {

#define CHECK_CALLSITE(frame, method)                                         \
  CHECK_RECEIVER(JSObject, receiver, method);                                 \
  LookupIterator it(isolate, receiver,                                        \
                    isolate->factory()->call_site_info_symbol(),              \
                    LookupIterator::OWN_SKIP_INTERCEPTOR);                    \
  if (it.state() != LookupIterator::DATA) {                                   \
    THROW_NEW_ERROR_RETURN_FAILURE(                                           \
        isolate,                                                              \
        NewTypeError(MessageTemplate::kCallSiteMethod,                        \
                     isolate->factory()->NewStringFromAsciiChecked(method))); \
  }                                                                           \
  Handle<CallSiteInfo> frame = Handle<CallSiteInfo>::cast(it.GetDataValue())
namespace {

Object PositiveNumberOrNull(int value, Isolate* isolate) {
  if (value > 0) return *isolate->factory()->NewNumberFromInt(value);
  return ReadOnlyRoots(isolate).null_value();
}

// The replay can compile scripts with different contents than were recorded
// (instrumentation does), which moves their frames. Everything that page code
// can read about a frame's position is recorded, so it sees the positions it
// saw while being recorded, whatever it computes from them.
bool ShouldRecordReplayCallSite(const char* why) {
  return recordreplay::IsRecordingOrReplaying(why) &&
         !recordreplay::AreEventsDisallowed();
}

int RecordReplayCallSiteInt(const char* why, int value) {
  if (!ShouldRecordReplayCallSite(why)) return value;
  return static_cast<int>(
      recordreplay::RecordReplayValue(why, static_cast<uintptr_t>(value)));
}

Handle<String> RecordReplayCallSiteString(const char* why, Isolate* isolate,
                                          Handle<String> value) {
  if (!ShouldRecordReplayCallSite(why)) return value;
  return replayio::RecordReplayStringHandle(why, isolate, value);
}

}  // namespace

BUILTIN(CallSitePrototypeGetColumnNumber) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getColumnNumber");
  return PositiveNumberOrNull(
      RecordReplayCallSiteInt("CallSite::getColumnNumber",
                              CallSiteInfo::GetColumnNumber(frame)),
      isolate);
}

BUILTIN(CallSitePrototypeGetEnclosingColumnNumber) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getEnclosingColumnNumber");
  return PositiveNumberOrNull(
      RecordReplayCallSiteInt("CallSite::getEnclosingColumnNumber",
                              CallSiteInfo::GetEnclosingColumnNumber(frame)),
      isolate);
}

BUILTIN(CallSitePrototypeGetEnclosingLineNumber) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getEnclosingLineNumber");
  return PositiveNumberOrNull(
      RecordReplayCallSiteInt("CallSite::getEnclosingLineNumber",
                              CallSiteInfo::GetEnclosingLineNumber(frame)),
      isolate);
}

BUILTIN(CallSitePrototypeGetEvalOrigin) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getEvalOrigin");
  Handle<PrimitiveHeapObject> origin = CallSiteInfo::GetEvalOrigin(frame);
  const char* why = "CallSite::getEvalOrigin";
  if (!ShouldRecordReplayCallSite(why)) return *origin;
  if (!recordreplay::RecordReplayValue(why, origin->IsString())) {
    return ReadOnlyRoots(isolate).undefined_value();
  }
  return *RecordReplayCallSiteString(
      why, isolate,
      origin->IsString() ? Handle<String>::cast(origin)
                         : isolate->factory()->empty_string());
}

BUILTIN(CallSitePrototypeGetFileName) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getFileName");
  return frame->GetScriptName();
}

BUILTIN(CallSitePrototypeGetFunction) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getFunction");
  if (frame->IsStrict() ||
      (frame->function().IsJSFunction() &&
       JSFunction::cast(frame->function()).shared().is_toplevel())) {
    return ReadOnlyRoots(isolate).undefined_value();
  }
  isolate->CountUsage(v8::Isolate::kCallSiteAPIGetFunctionSloppyCall);
  return frame->function();
}

BUILTIN(CallSitePrototypeGetFunctionName) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getFunctionName");
  return *CallSiteInfo::GetFunctionName(frame);
}

BUILTIN(CallSitePrototypeGetLineNumber) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getLineNumber");
  return PositiveNumberOrNull(
      RecordReplayCallSiteInt("CallSite::getLineNumber",
                              CallSiteInfo::GetLineNumber(frame)),
      isolate);
}

BUILTIN(CallSitePrototypeGetMethodName) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getMethodName");
  return *CallSiteInfo::GetMethodName(frame);
}

BUILTIN(CallSitePrototypeGetPosition) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getPosition");
  return Smi::FromInt(RecordReplayCallSiteInt(
      "CallSite::getPosition", CallSiteInfo::GetSourcePosition(frame)));
}

BUILTIN(CallSitePrototypeGetPromiseIndex) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getPromiseIndex");
  if (!frame->IsPromiseAll() && !frame->IsPromiseAny() &&
      !frame->IsPromiseAllSettled()) {
    return ReadOnlyRoots(isolate).null_value();
  }
  return Smi::FromInt(CallSiteInfo::GetSourcePosition(frame));
}

BUILTIN(CallSitePrototypeGetScriptHash) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getScriptHash");
  return *RecordReplayCallSiteString("CallSite::getScriptHash", isolate,
                                     CallSiteInfo::GetScriptHash(frame));
}

BUILTIN(CallSitePrototypeGetScriptNameOrSourceURL) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getScriptNameOrSourceUrl");
  return frame->GetScriptNameOrSourceURL();
}

BUILTIN(CallSitePrototypeGetThis) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getThis");
  if (frame->IsStrict()) return ReadOnlyRoots(isolate).undefined_value();
  isolate->CountUsage(v8::Isolate::kCallSiteAPIGetThisSloppyCall);
#if V8_ENABLE_WEBASSEMBLY
  if (frame->IsAsmJsWasm()) {
    return frame->GetWasmInstance().native_context().global_proxy();
  }
#endif  // V8_ENABLE_WEBASSEMBLY
  return frame->receiver_or_instance();
}

BUILTIN(CallSitePrototypeGetTypeName) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "getTypeName");
  return *CallSiteInfo::GetTypeName(frame);
}

BUILTIN(CallSitePrototypeIsAsync) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "isAsync");
  return isolate->heap()->ToBoolean(frame->IsAsync());
}

BUILTIN(CallSitePrototypeIsConstructor) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "isConstructor");
  return isolate->heap()->ToBoolean(frame->IsConstructor());
}

BUILTIN(CallSitePrototypeIsEval) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "isEval");
  return isolate->heap()->ToBoolean(frame->IsEval());
}

BUILTIN(CallSitePrototypeIsNative) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "isNative");
  return isolate->heap()->ToBoolean(frame->IsNative());
}

BUILTIN(CallSitePrototypeIsPromiseAll) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "isPromiseAll");
  return isolate->heap()->ToBoolean(frame->IsPromiseAll());
}

BUILTIN(CallSitePrototypeIsToplevel) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "isToplevel");
  return isolate->heap()->ToBoolean(frame->IsToplevel());
}

BUILTIN(CallSitePrototypeToString) {
  HandleScope scope(isolate);
  CHECK_CALLSITE(frame, "toString");
  Handle<String> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     SerializeCallSiteInfo(isolate, frame));
  return *RecordReplayCallSiteString("CallSite::toString", isolate, result);
}

#undef CHECK_CALLSITE

}  // namespace internal
}  // namespace v8
