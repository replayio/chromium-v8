#include "include/replayio.h"
#include "src/replayio/replayio-api.h"
#include "src/replayio/replayio-util.h"
#include "src/api/api-inl.h"
#include "src/json/json-parser.h"
#include "src/json/json-stringifier.h"

namespace v8 {
namespace i = internal;
namespace internal {
  extern void RecordReplayOnNewSource(Isolate* isolate, const char* id,
                                      const char* kind, const char* url);
}
namespace replayio {

extern "C" void V8RecordReplayEnterReplayCode();
extern "C" void V8RecordReplayExitReplayCode();
  
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

  // TODO: Call this on all live root contexts.
  replayio::ReplayRootContext* rootContext = RecordReplayGetRootContext();
  if (!rootContext) {
    return;
  }

  i::Isolate* isolate = i::Isolate::Current();
  base::Optional<i::SaveAndSwitchContext> ssc;
  EnsureIsolateContext(isolate, ssc);

  std::string callbackName = "clearPauseData";
  rootContext->EmitReplayEvent(callbackName);
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

/** ###########################################################################
 * Replay Script Registry.
 * ##########################################################################*/

// Map ScriptId => Script. We keep all scripts around forever when recording/replaying.
ScriptIdMap* gRecordReplayScripts;
ScriptIdSet* gRegisteredScripts;

bool RecordReplayHasRegisteredScript(i::Script script) {
  return IsMainThread() &&
    gRegisteredScripts &&
    gRegisteredScripts->find(script.id()) != gRegisteredScripts->end();
}

bool RecordReplayIsDivergentUserJSWithoutPause(
    const i::SharedFunctionInfo& shared) {
  return recordreplay::AreEventsDisallowed() &&
         !recordreplay::HasDivergedFromRecording() &&
         shared.script().IsScript() &&
         RecordReplayHasRegisteredScript(
             i::Script::cast(shared.script()));
}

// Get the script from an ID.
i::MaybeHandle<i::Script> MaybeGetScript(i::Isolate* isolate, int script_id) {
  CHECK(gRecordReplayScripts);
  auto iter = gRecordReplayScripts->find(script_id);
  if (iter == gRecordReplayScripts->end()) {
    return i::MaybeHandle<i::Script>();
  }

  Local<v8::Value> scriptValue = iter->second.Get((v8::Isolate*)isolate);
  i::Handle<i::Object> scriptObj = Utils::OpenHandle(*scriptValue);
  i::Handle<i::Script> script(i::Script::cast(*scriptObj), isolate);
  CHECK(script->id() == script_id);
  return script;
};

static i::Handle<i::String> GetProtocolSourceId(i::Isolate* isolate, i::Handle<i::Script> script) {
  std::ostringstream os;
  os << script->id();
  return CStringToHandle(isolate, os.str().c_str());
}

static void RecordReplayRegisterScript(i::Handle<i::Script> script) {
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

  i::Isolate* isolate = i::Isolate::Current();

  (*gRecordReplayScripts)[script->id()] =
    Eternal<Value>((v8::Isolate*)isolate, v8::Utils::ToLocal(script));

  // TODO: Pick the root from isolate->context()
  if (!RecordReplayHasDefaultContext()) {
    return;
  }

  i::Handle<i::String> idStr = GetProtocolSourceId(isolate, script);
  std::unique_ptr<char[]> id = i::String::cast(*idStr).ToCString();

#if V8_ENABLE_WEBASSEMBLY
  if (script->type() == i::Script::TYPE_WASM) {
    return;
  }
#endif

  if (recordreplay::AreEventsDisallowed()) {
    return;
  }

  std::string url;
  if (!script->name().IsUndefined()) {
    std::unique_ptr<char[]> name = i::String::cast(script->name()).ToCString();
    url = name.get();
  }

  if (!RecordReplayIsInternalScriptURL(url.c_str())) {
    RecordReplayAddInterestingSource(url.c_str());
  }

  // It's not clear how we can distinguish inline scripts from HTML files vs.
  // scripts loaded in other ways here. Use the initial position of the script
  // to distinguish these cases: if the starting position is anything other
  // than line zero / column zero, the script must be inlined into another file.
  i::Script::PositionInfo start_info;
  i::Script::GetPositionInfo(script, 0, &start_info, i::Script::WITH_OFFSET);

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
      i::Handle<i::Object> handler = Utils::OpenHandle(*handler_value);

      i::Handle<i::Object> callArgs[3];
      callArgs[0] = idStr;
      callArgs[1] = i::Handle<i::Object>(script->GetNameOrSourceURL(), isolate);
      callArgs[2] = i::Handle<i::Object>(script->source_mapping_url(), isolate);
      i::Handle<i::Object> undefined = isolate->factory()->undefined_value();
      
      v8::TryCatch try_catch((v8::Isolate*)isolate);
      i::Handle<i::JSFunction> function = i::Handle<i::JSFunction>::cast(handler);
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

      i::MaybeHandle<i::Object> newScriptHandlerResult = i::Execution::Call(isolate, handler, undefined, 3, callArgs);
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

  i::RecordReplayOnNewSource(isolate, id.get(), kind, url.length() ? url.c_str() : nullptr);
}


/** ###########################################################################
 * CommandCallback
 * ##########################################################################*/

// Command callbacks which we handle directly.
struct InternalCommandCallback {
  const char* mCommand;
  i::Handle<i::Object> (*mCallback)(i::Isolate* isolate, i::Handle<i::Object> params);
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
static void EnsureIsolateContext(i::Isolate* isolate, base::Optional<i::SaveAndSwitchContext>& ssc) {
  // TODO: Get the correct root context based on the current context and the current situation!
  // TODO: Check Isolate::has_pending_exception()
  if (isolate->context().is_null()) {
    Local<v8::Context> v8_context;
    V8RecordReplayGetDefaultContext((v8::Isolate*)isolate, &v8_context);
    i::Handle<i::Context> context = Utils::OpenHandle(*v8_context);
    ssc.emplace(isolate, *context);
  }
}


char* CommandCallback(const char* command, const char* params) {
  CHECK(IsMainThread());
  AutoMarkReplayCode amrc;
  uint64_t startProgressCounter = *gProgressCounter;
  replayio::AutoDisallowEvents disallow("CommandCallback");

  i::Isolate* isolate = i::Isolate::Current();

  base::Optional<i::SaveAndSwitchContext> ssc;
  EnsureIsolateContext(isolate, ssc);

  i::HandleScope scope(isolate);

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


  i::Handle<i::Object> undefined = isolate->factory()->undefined_value();
  i::Handle<i::String> paramsStr = CStringToHandle(isolate, params);

  i::MaybeHandle<i::Object> maybeParams = i::JsonParser<uint8_t>::Parse(isolate, paramsStr, undefined);
  if (maybeParams.is_null()) {
    recordreplay::Crash("Error: CommandCallback Parse %s failed", params);
  }
  i::Handle<i::Object> paramsObj = maybeParams.ToHandleChecked();

  i::MaybeHandle<i::Object> rv;
  for (const InternalCommandCallback& cb : gInternalCommandCallbacks) {
    if (!strcmp(cb.mCommand, command)) {
      rv = cb.mCallback(isolate, paramsObj);
      if (rv.is_null()) {
        recordreplay::Crash("Error: CommandCallback internal command %s failed", command);
      }
    }
  }
  if (rv.is_null()) {
    // Handle in with the JS command handler.
    ReplayRootContext* root = RecordReplayGetRootContext();
    CHECK(root);
    Local<Context> cx = root->GetContext();
    Local<Object> callArgs = v8::Object::New((v8::Isolate*)isolate);
    std::string callbackName = "command";
    Local<Value> result = root->EmitReplayEvent(
      callbackName,
      Utils::ToLocal(paramsObj)
    );
  }

  i::Handle<i::Object> result = rv.ToHandleChecked();
  i::Handle<i::Object> rvStr = i::JsonStringify(isolate, result, undefined, undefined).ToHandleChecked();
  std::unique_ptr<char[]> rvCStr = i::String::cast(*rvStr).ToCString();

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