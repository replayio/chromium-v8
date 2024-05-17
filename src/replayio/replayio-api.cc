#include "include/replayio.h"
#include "src/execution/isolate-inl.h"

namespace v8 {
namespace i = internal;
namespace replayio {
  
struct AutoMarkReplayCode {
  AutoMarkReplayCode() {
    V8RecordReplayEnterReplayCode();
  }
  ~AutoMarkReplayCode() {
    V8RecordReplayExitReplayCode();
  }
};

void ClearPauseDataCallback() {
  CHECK(IsMainThread());
  AutoMarkReplayCode amrc;
  replayio::AutoDisallowEvents disallow("ClearPauseDataCallback");

  if (!gClearPauseDataCallback) {
    return;
  }

  Isolate* isolate = Isolate::Current();
  base::Optional<SaveAndSwitchContext> ssc;
  EnsureIsolateContext(isolate, ssc);

  HandleScope scope(isolate);

  Local<v8::Value> callbackValue = gClearPauseDataCallback->Get((v8::Isolate*)isolate);
  Handle<Object> callback = Utils::OpenHandle(*callbackValue);

  Handle<Object> undefined = isolate->factory()->undefined_value();
  MaybeHandle<Object> rv = Execution::Call(isolate, callback, undefined, 0, nullptr);
  CHECK(!rv.is_null());
}

extern void RecordReplayAddInterestingSource(const char* url);

// Return whether a script is an uninteresting internal URL, but which still needs
// to be registered with the recorder so that breakpoints can be created.
bool RecordReplayIsInternalScriptURL(const char* url) {
  return !strcmp(url, "record-replay-react-devtools") ||
         !strcmp(url, "record-replay-internal") ||
         !strncmp(url, "extensions::", 12);
}

extern bool RecordReplayHasDefaultContext();

typedef std::unordered_set<int> ScriptIdSet;
static ScriptIdSet* gRegisteredScripts;

bool RecordReplayHasRegisteredScript(Script script) {
  return IsMainThread() &&
    gRegisteredScripts &&
    gRegisteredScripts->find(script.id()) != gRegisteredScripts->end();
}

bool RecordReplayIsDivergentUserJSWithoutPause(
    const SharedFunctionInfo& shared) {
  return recordreplay::AreEventsDisallowed() &&
         !recordreplay::HasDivergedFromRecording() &&
         shared.script().IsScript() &&
         RecordReplayHasRegisteredScript(
             Script::cast(shared.script()));
}

extern "C" void V8RecordReplayEnterReplayCode();
extern "C" void V8RecordReplayExitReplayCode();

static void RecordReplayRegisterScript(Handle<Script> script) {
  AutoMarkReplayCode amrc;
  CHECK(IsMainThread());

  if (!gRecordReplayScripts) {
    gRecordReplayScripts = new ScriptIdMap();
  }
  auto iter = gRecordReplayScripts->find(script->id());
  if (iter != gRecordReplayScripts->end()) {
    // Ignore duplicate registers.
    return;
  }

  i::Isolate* isolate = Isolate::Current();

  (*gRecordReplayScripts)[script->id()] =
    Eternal<Value>((v8::Isolate*)isolate, v8::Utils::ToLocal(script));

  if (!RecordReplayHasDefaultContext()) {
    return;
  }

  Handle<String> idStr = GetProtocolSourceId(isolate, script);
  std::unique_ptr<char[]> id = String::cast(*idStr).ToCString();

  if (script->type() == Script::TYPE_WASM) {
    return;
  }

  if (recordreplay::AreEventsDisallowed()) {
    return;
  }

  std::string url;
  if (!script->name().IsUndefined()) {
    std::unique_ptr<char[]> name = String::cast(script->name()).ToCString();
    url = name.get();
  }

  if (!RecordReplayIsInternalScriptURL(url.c_str())) {
    RecordReplayAddInterestingSource(url.c_str());
  }

  // It's not clear how we can distinguish inline scripts from HTML files vs.
  // scripts loaded in other ways here. Use the initial position of the script
  // to distinguish these cases: if the starting position is anything other
  // than line zero / column zero, the script must be inlined into another file.
  Script::PositionInfo start_info;
  Script::GetPositionInfo(script, 0, &start_info, Script::WITH_OFFSET);

  // [RUN-2172] Blink-internal scripts sometimes might have line or column, but 
  // no URL. Since the backend requires inlineScripts to have a URL, don't flag 
  // it as such.
  const char* kind = ((start_info.line || start_info.column) && !url.empty())
                         ? "inlineScript"
                         : "scriptSource";

  recordreplay::Diagnostic("OnNewSource %s %s", id.get(), kind);

  if (!gRegisteredScripts) {
    gRegisteredScripts = new ScriptIdSet;
  }
  gRegisteredScripts->insert(script->id());

  if (gNewScriptHandlers) {
    for (auto entry : *gNewScriptHandlers) {
      auto handlerEternalValue = entry.handler;
      auto disallowEvents = entry.disallowEvents;

      base::Optional<replayio::AutoDisallowEvents> disallow;
      if (disallowEvents) {
        disallow.emplace("RecordReplayRegisterScript");
      }

      Local<v8::Object> handler_value = handlerEternalValue->Get((v8::Isolate*)isolate);
      Handle<Object> handler = Utils::OpenHandle(*handler_value);

      Handle<Object> callArgs[3];
      callArgs[0] = idStr;
      callArgs[1] = Handle<Object>(script->GetNameOrSourceURL(), isolate);
      callArgs[2] = Handle<Object>(script->source_mapping_url(), isolate);
      Handle<Object> undefined = isolate->factory()->undefined_value();
      
      v8::TryCatch try_catch((v8::Isolate*)isolate);
      Handle<JSFunction> function = Handle<JSFunction>::cast(handler);
      i::Handle<i::Context> context_handle = i::Handle<i::Context>(
        function->context().script_context(),
        isolate
      );
      v8::MaybeLocal<v8::Context> receiver_context = handler_value->GetCreationContext();
      if (receiver_context.IsEmpty()) {
        // Make sure we never call a callback whose creation context is gone.
        // TODO: Find out if function->context() and GetCreationContext() are always assured to have the same root?
        recordreplay::Warning("[TT-1112] NewScriptHandler_context is gone");
        continue;
      }

      MaybeHandle<Object> newScriptHandlerResult = Execution::Call(isolate, handler, undefined, 3, callArgs);
      if (try_catch.HasCaught()) {
        Local<v8::Context> context = Utils::ToLocal(context_handle);
        Local<Message> msg = try_catch.Message();
        v8::String::Utf8Value msgString((v8::Isolate*)isolate, msg->Get());
        recordreplay::Crash("NewScriptHandler call failed (%d:%d): %s",
                            msg->GetLineNumber(context).FromMaybe(-1),
                            msg->GetStartColumn(context).FromJust(),
                            *msgString
        );
      }
      CHECK(!newScriptHandlerResult.is_null());
    }
  }

  RecordReplayOnNewSource(isolate, id.get(), kind, url.length() ? url.c_str() : nullptr);
}

// Command callbacks which we handle directly.
struct InternalCommandCallback {
  const char* mCommand;
  Handle<Object> (*mCallback)(Isolate* isolate, Handle<Object> params);
};
static InternalCommandCallback gInternalCommandCallbacks[] = {
  { "Debugger.getSourceContents", RecordReplayGetSourceContents },
  { "Debugger.getPossibleBreakpoints", RecordReplayGetPossibleBreakpoints },
  { "Target.convertLocationToFunctionOffset", RecordReplayConvertLocationToFunctionOffset },
  { "Target.convertFunctionOffsetToLocation", RecordReplayConvertFunctionOffsetToLocation },
  { "Target.countStackFrames", RecordReplayCountStackFrames },
  { "Target.getFunctionsInRange", RecordReplayGetFunctionsInRange },
  { "Target.getHTMLSource", RecordReplayGetHTMLSource },
  { "Target.currentGeneratorId", RecordReplayCurrentGeneratorId },
};

extern "C" void V8RecordReplayGetDefaultContext(v8::Isolate* isolate, v8::Local<v8::Context>* cx);
extern uint64_t* gProgressCounter;
extern int gRecordReplayCheckProgress;
static int gPauseContextGroupId = 0;

// Make sure that the isolate has a context by switching to the default
// context if necessary.
static void EnsureIsolateContext(Isolate* isolate, base::Optional<SaveAndSwitchContext>& ssc) {
  if (isolate->context().is_null()) {
    Local<v8::Context> v8_context;
    V8RecordReplayGetDefaultContext((v8::Isolate*)isolate, &v8_context);
    Handle<Context> context = Utils::OpenHandle(*v8_context);
    ssc.emplace(isolate, *context);
  }
}


char* CommandCallback(const char* command, const char* params) {
  CHECK(IsMainThread());
  AutoMarkReplayCode amrc;
  uint64_t startProgressCounter = *gProgressCounter;
  replayio::AutoDisallowEvents disallow("CommandCallback");

  Isolate* isolate = Isolate::Current();

  // TODO: This won't work as expected, since Execution::Call always enters
  //       the context of the compiled function.
  //      → See |if (params.target->IsJSFunction())| in execution.cc.
  // base::Optional<SaveAndSwitchContext> ssc;
  // EnsureIsolateContext(isolate, ssc);

  HandleScope scope(isolate);

  // if (recordreplay::HasDivergedFromRecording()) {
  //   v8_inspector::V8Inspector* inspectorRaw = v8::debug::GetInspector((v8::Isolate*)isolate);
  //   int currentGroupId;
  //   if (!inspectorRaw) {
  //     currentGroupId = -1;
  //   } else {
  //     v8_inspector::V8InspectorImpl* inspector =
  //         static_cast<v8_inspector::V8InspectorImpl*>(inspectorRaw);
  //     int contextId = v8_inspector::InspectedContext::contextId(
  //       ((v8::Isolate*)isolate)->GetCurrentContext()
  //     );
  //     currentGroupId = inspector->contextGroupId(contextId);
  //   }
  //   if (!gPauseContextGroupId) {
  //     gPauseContextGroupId = currentGroupId;
  //   } else {
  //     // [RUN-3123] Don't allow querying different context groups on the
  //     // same pause.
  //     CHECK(gPauseContextGroupId == currentGroupId);
  //   }
  // }


  Handle<Object> undefined = isolate->factory()->undefined_value();
  Handle<String> paramsStr = CStringToHandle(isolate, params);

  MaybeHandle<Object> maybeParams = JsonParser<uint8_t>::Parse(isolate, paramsStr, undefined);
  if (maybeParams.is_null()) {
    recordreplay::Crash("Error: CommandCallback Parse %s failed", params);
  }
  Handle<Object> paramsObj = maybeParams.ToHandleChecked();

  MaybeHandle<Object> rv;
  for (const InternalCommandCallback& cb : gInternalCommandCallbacks) {
    if (!strcmp(cb.mCommand, command)) {
      rv = cb.mCallback(isolate, paramsObj);
      if (rv.is_null()) {
        recordreplay::Crash("Error: CommandCallback internal command %s failed", command);
      }
    }
  }
  if (rv.is_null()) {
    CHECK(gCommandCallback);
    Local<v8::Function> callbackValue = gCommandCallback->Get((v8::Isolate*)isolate);
    Handle<Object> callback = Utils::OpenHandle(*callbackValue);

    Handle<Object> callArgs[2];
    callArgs[0] = CStringToHandle(isolate, command);
    callArgs[1] = paramsObj;
    rv = Execution::Call(isolate, callback, undefined, 2, callArgs);
    if (rv.is_null()) {
      recordreplay::Crash("Error: CommandCallback generic command %s failed", command);
    }
  }

  Handle<Object> result = rv.ToHandleChecked();
  Handle<Object> rvStr = JsonStringify(isolate, result, undefined, undefined).ToHandleChecked();
  std::unique_ptr<char[]> rvCStr = String::cast(*rvStr).ToCString();

  if (startProgressCounter < *gProgressCounter && !recordreplay::HasDivergedFromRecording()) {
    // [RUN-1988] Our command handler incremented the PC by accidentally calling
    // into instrumented user code.
    // Note that a warning has already been generated due to
    // gRecordReplayCheckProgress being set.
    // → Let's reset the PC.
    *gProgressCounter = startProgressCounter;
  }

  return strdup(rvCStr.get());
}


}  // namespace replayio
}  // namespace v8