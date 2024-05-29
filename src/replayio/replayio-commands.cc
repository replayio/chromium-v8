#include <unordered_set>
#include "include/replayio.h"
#include "src/api/api-inl.h"
#include "src/codegen/compiler.h"
#include "src/json/json-parser.h"
#include "src/json/json-stringifier.h"
#include "src/objects/js-collection-inl.h"
#include "src/replayio/replayio-commands.h"
#include "src/replayio/replayio-util.h"

using v8::replayio::CStringToHandle;

namespace v8 {
namespace i = internal;
namespace internal {
extern void RecordReplayOnNewSource(Isolate* isolate, const char* id,
                                    const char* kind, const char* url);

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

extern void RecordReplayAddInterestingSource(const char* url);

// Return whether a script is an uninteresting internal URL, but which still needs
// to be registered with the recorder so that breakpoints can be created.
bool RecordReplayIsInternalScriptURL(const char* url) {
  return !strcmp(url, "record-replay-react-devtools") ||
         replayio::RecordReplayIsInternalReplayJs(url) ||
         !strncmp(url, "extensions::", 12);
}
extern void RecordReplayAddPossibleBreakpoint(int line, int column, const char* function_id, int function_index);


// Record Replay handlers and associated helpers. These ought to be in their
// own file, but it's easier to put them here.

////////////////////////////////////////////////////////////////////////////////
// Helpers
////////////////////////////////////////////////////////////////////////////////

static Handle<Object> GetProperty(Isolate* isolate,
                                  Handle<Object> obj, const char* property) {
  return Object::GetProperty(isolate, obj, CStringToHandle(isolate, property))
    .ToHandleChecked();
}

static void SetProperty(Isolate* isolate,
                        Handle<Object> obj, const char* property,
                        Handle<Object> value) {
  Object::SetProperty(isolate, obj,
                      CStringToHandle(isolate, property), value).Check();
}

static void SetProperty(Isolate* isolate,
                        Handle<Object> obj, const char* property,
                        const char* value) {
  SetProperty(isolate, obj, property, CStringToHandle(isolate, value));
}

static void SetProperty(Isolate* isolate,
                        Handle<Object> obj, const char* property,
                        double value) {
  SetProperty(isolate, obj, property, isolate->factory()->NewNumber(value));
}

static Handle<JSObject> NewPlainObject(Isolate* isolate) {
  return isolate->factory()->NewJSObject(isolate->object_function());
}

/** ###########################################################################
 * Replay Script Registry.
 * ##########################################################################*/

using ScriptIdMap = std::unordered_map<int, Eternal<Value>>;
using ScriptIdSet = std::unordered_set<int>;
// Map ScriptId => Script. We keep all scripts around forever when recording/replaying.
ScriptIdMap* gRecordReplayScripts;
ScriptIdSet* gRegisteredScripts;

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
}

// Get the script from an ID.
Handle<Script> GetScript(Isolate* isolate, int script_id) {
  MaybeHandle<Script> script = MaybeGetScript(isolate, script_id);
  if (script.is_null()) {
    recordreplay::Diagnostic("GetScript unknown script %d", script_id);
  }
  return script.ToHandleChecked();
}

static int GetSourceIdProperty(Isolate* isolate, Handle<Object> obj) {
  Handle<Object> sourceIdStr = GetProperty(isolate, obj, "sourceId");
  std::unique_ptr<char[]> sourceIdText = String::cast(*sourceIdStr).ToCString();
  int rv = atoi(sourceIdText.get());
  recordreplay::Diagnostic("GetSourceIdProperty %s %d", sourceIdText.get(), rv);
  return rv;
}

static void DecodeLocationProperty(Isolate* isolate, Handle<Object> params,
                                   const char* property, int* line, int* column) {
  Handle<Object> location = GetProperty(isolate, params, property);
  if (location->IsUndefined()) {
    return;
  }

  Handle<Object> lineProperty = GetProperty(isolate, location, "line");
  *line = lineProperty->Number();

  Handle<Object> columnProperty = GetProperty(isolate, location, "column");
  *column = columnProperty->Number();
}

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

static Handle<String> GetProtocolSourceId(Isolate* isolate, Handle<Script> script) {
  std::ostringstream os;
  os << script->id();
  return CStringToHandle(isolate, os.str().c_str());
}

void RecordReplayRegisterScript(i::Handle<i::Script> script) {
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

  CHECK(!isolate->context().is_null());
  
  i::Handle<i::Context> cx_handle = i::Handle<i::Context>(isolate->context(), isolate);
  Local<v8::Context> context = Utils::ToLocal(cx_handle);
  replayio::ReplayRootContext* root = replayio::RecordReplayGetRootContext(context);
  if (!root) {
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

  if (!replayio::RecordReplayIsInternalReplayJs(url.c_str())) {
    // The first "internal Replay JS" script initializes the event emitter.
    // Since this runs beforehand, we have a chicken-and-egg problem.
    // Resolve this by simply not emitting newScript events for internal
    // JS scripts for now.
    constexpr int argc = 3;
    Local<Value> callArgs[argc];
    callArgs[0] = Utils::ToLocal(idStr);
    callArgs[1] = Utils::ToLocal(i::Handle<i::Object>(script->GetNameOrSourceURL(), isolate));
    callArgs[2] = Utils::ToLocal(i::Handle<i::Object>(script->source_mapping_url(), isolate));

    {
      root->EmitReplayEvent("newScript", argc, callArgs);
      
      replayio::AutoDisallowEvents disallow("RecordReplayRegisterScript");
      root->EmitReplayEvent("newScriptEventsDisallowed", argc, callArgs);
    }
  }

  i::RecordReplayOnNewSource(isolate, id.get(), kind, url.length() ? url.c_str() : nullptr);
}

extern "C" void V8RecordReplayGetDefaultContext(v8::Isolate* isolate, v8::Local<v8::Context>* cx);
extern uint64_t* gProgressCounter;
extern int gRecordReplayCheckProgress;

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


/** ###########################################################################
 * ForEachInstrumentationOp*.
 * ##########################################################################*/

extern const char* InstrumentationSiteKind(int index);
extern int InstrumentationSiteSourcePosition(int index);
extern int InstrumentationSiteFunctionIndex(int index);
extern std::string GetRecordReplayFunctionId(Handle<SharedFunctionInfo> shared);


static void GetInstrumentationSiteLocation(Handle<Script> script, int instrumentation_index,
                                           int* pline, int* pcolumn) {
  int source_position = InstrumentationSiteSourcePosition(instrumentation_index);
  Script::PositionInfo info;
  Script::GetPositionInfo(script, source_position, &info, Script::WITH_OFFSET);

  // Use 1-indexed lines instead of 0-indexed.
  *pline = info.line + 1;
  *pcolumn = info.column;
}

static void ForEachInstrumentationOp(Isolate* isolate, Handle<Script> script,
                                     std::function<void(Handle<SharedFunctionInfo>,
                                                        int)> aCallback) {
  // Based on Debug::GetPossibleBreakpoints.
  while (true) {
    HandleScope scope(isolate);
    std::vector<Handle<SharedFunctionInfo>> candidates;
    std::vector<IsCompiledScope> compiled_scopes;
    SharedFunctionInfo::ScriptIterator iterator(isolate, *script);
    for (SharedFunctionInfo info = iterator.Next(); !info.is_null();
         info = iterator.Next()) {
      if (!info.IsSubjectToDebugging()) continue;
      if (!info.is_compiled() && !info.allows_lazy_compilation()) continue;
      candidates.push_back(i::handle(info, isolate));
    }

    // Compile any uncompiled functions found in the script.
    bool was_compiled = false;
    for (const auto& candidate : candidates) {
      IsCompiledScope is_compiled_scope(candidate->is_compiled_scope(isolate));
      if (!is_compiled_scope.is_compiled()) {
        if (!Compiler::Compile(isolate, candidate, Compiler::CLEAR_EXCEPTION,
                               &is_compiled_scope)) {
          V8_Fatal("Compiler::Compile failed in ForEachInstrumentationOp.");
        } else {
          was_compiled = true;
        }
      }
      DCHECK(is_compiled_scope.is_compiled());
      compiled_scopes.push_back(is_compiled_scope);
    }

    // If we did any compilation, restart and look for any new functions
    // that need to be compiled.
    if (was_compiled) continue;

    // Now we have a complete list of the functions in the script.
    // Build the final locations.
    for (const auto& candidate : candidates) {
      if (!candidate->HasBytecodeArray()) {
        continue;
      }
      Handle<BytecodeArray> bytecode(candidate->GetBytecodeArray(isolate), isolate);

      for (interpreter::BytecodeArrayIterator it(bytecode); !it.done();
           it.Advance()) {
        interpreter::Bytecode bytecode = it.current_bytecode();
        if (bytecode == interpreter::Bytecode::kRecordReplayInstrumentation ||
            bytecode == interpreter::Bytecode::kRecordReplayInstrumentationGenerator ||
            bytecode == interpreter::Bytecode::kRecordReplayInstrumentationReturn) {
          int index = it.GetIndexOperand(0);
          aCallback(candidate, index);
        }
      }
    }
    return;
  }
}

static void ForEachInstrumentationOpInRange(
  Isolate* isolate, Handle<Object> params,
  const std::function<void(Handle<Script> script, int bytecode_offset,
                           const std::string& function_id, int line, int column)> callback) {
  int script_id = GetSourceIdProperty(isolate, params);
  MaybeHandle<Script> maybe_script = MaybeGetScript(isolate, script_id);

  if (maybe_script.is_null()) {
    return;
  }

  Handle<Script> script = maybe_script.ToHandleChecked();

  int beginLine = 1, beginColumn = 0;
  DecodeLocationProperty(isolate, params, "begin", &beginLine, &beginColumn);

  int endLine = INT32_MAX, endColumn = INT32_MAX;
  DecodeLocationProperty(isolate, params, "end", &endLine, &endColumn);

  ForEachInstrumentationOp(isolate, script, [&](Handle<SharedFunctionInfo> shared,
                                                int instrumentation_index) {
    if (strcmp(InstrumentationSiteKind(instrumentation_index), "breakpoint")) {
      return;
    }

    int line, column;
    GetInstrumentationSiteLocation(script, instrumentation_index, &line, &column);

    if (line < beginLine ||
        (line == beginLine && column < beginColumn) ||
        line > endLine ||
        (line == endLine && column > endColumn)) {
      return;
    }

    int bytecode_offset = InstrumentationSiteFunctionIndex(instrumentation_index);

    std::string function_id = GetRecordReplayFunctionId(shared);
    callback(script, bytecode_offset, function_id, line, column);
  });
}

// Information about breakpoints that have been sent to the record replay driver.
struct BreakpointInfo {
  std::string function_id_;
  int bytecode_offset_;
  BreakpointInfo(const std::string& function_id, int bytecode_offset)
    : function_id_(function_id), bytecode_offset_(bytecode_offset) {}
};
typedef std::unordered_map<std::string, BreakpointInfo> BreakpointInfoMap;
static BreakpointInfoMap* gBreakpoints;

static std::string BreakpointKey(int script_id, int line, int column) {
  std::ostringstream os;
  os << script_id << ":" << line << ":" << column;
  return os.str();
}

// Inverse of gBreakpoints mapping.
struct BreakpointPosition {
  int line_;
  int column_;
  BreakpointPosition(int line, int column)
    : line_(line), column_(column) {}
};
typedef std::unordered_map<std::string, BreakpointPosition> BreakpointPositionMap;
static BreakpointPositionMap* gBreakpointPositions;

static std::string BreakpointPositionKey(std::string function_id,
                                         int bytecode_offset) {
  std::ostringstream os;
  os << function_id << ":" << bytecode_offset;
  return os.str();
}

static void GenerateBreakpointInfo(Isolate* isolate, Handle<Script> script) {
  if (!gBreakpoints) {
    gBreakpoints = new BreakpointInfoMap();
  }
  if (!gBreakpointPositions) {
    gBreakpointPositions = new BreakpointPositionMap();
  }

  ForEachInstrumentationOp(isolate, script, [&](Handle<SharedFunctionInfo> shared,
                                                int instrumentation_index) {
    int line, column;
    GetInstrumentationSiteLocation(script, instrumentation_index, &line, &column);

    std::string function_id = GetRecordReplayFunctionId(shared);
    int bytecode_offset = InstrumentationSiteFunctionIndex(instrumentation_index);

    std::string key = BreakpointKey(script->id(), line, column);
    BreakpointInfo value(function_id, bytecode_offset);
    gBreakpoints->insert(std::pair<std::string, BreakpointInfo>
                         (key, value));

    std::string positionKey = BreakpointPositionKey(function_id, bytecode_offset);
    BreakpointPosition position(line, column);
    gBreakpointPositions->insert(std::pair<std::string, BreakpointPosition>
                                 (positionKey, position));
  });
}

/** ###########################################################################
 * Internal command callbacks.
 * ##########################################################################*/

// Command callbacks which we handle directly.
i::Handle<i::Object> RecordReplayGetSourceContents(i::Isolate* isolate, i::Handle<i::Object> params) {
  int script_id = GetSourceIdProperty(isolate, params);
  recordreplay::Diagnostic("RecordReplayGetSourceContents #1 %d", script_id);
  Handle<Script> script = GetScript(isolate, script_id);
  recordreplay::Diagnostic("RecordReplayGetSourceContents #2");

  Script::PositionInfo info;
  Script::GetPositionInfo(script, 0, &info, Script::WITH_OFFSET);

  // Pad the start of the source with lines to adjust for its starting position.
  // Note that we don't pad the starting line with blank spaces so that columns
  // match up, in order to match the spidermonkey implementation.
  std::string padded_source;
  for (int i = 0; i < info.line; i++) {
    padded_source += "\n";
  }

  Handle<String> source(String::cast(script->source()), isolate);
  padded_source += source->ToCString().get();

  Handle<JSObject> obj = NewPlainObject(isolate);
  SetProperty(isolate, obj, "contents", padded_source.c_str());
  SetProperty(isolate, obj, "contentType", "text/javascript");
  return obj;
}
struct InternalCommandCallback {
  const char* mCommand;
  i::Handle<i::Object> (*mCallback)(i::Isolate* isolate, i::Handle<i::Object> params);
};

Handle<Object> RecordReplayConvertLocationToFunctionOffset(Isolate* isolate,
                                                           Handle<Object> params) {
  Handle<Object> location = GetProperty(isolate, params, "location");
  int sourceId = GetSourceIdProperty(isolate, location);
  int line = GetProperty(isolate, location, "line")->Number();
  int column = GetProperty(isolate, location, "column")->Number();

  std::string key = BreakpointKey(sourceId, line, column);
  if (!gBreakpoints) {
    Handle<Script> script = GetScript(isolate, sourceId);
    GenerateBreakpointInfo(isolate, script);
  }
  auto iter = gBreakpoints->find(key);
  if (iter == gBreakpoints->end()) {
    Handle<Script> script = GetScript(isolate, sourceId);
    GenerateBreakpointInfo(isolate, script);

    iter = gBreakpoints->find(key);
    if (iter == gBreakpoints->end()) {
      return NewPlainObject(isolate);
    }
  }

  Handle<JSObject> rv = NewPlainObject(isolate);
  SetProperty(isolate, rv, "functionId", iter->second.function_id_.c_str());
  SetProperty(isolate, rv, "offset", iter->second.bytecode_offset_);
  return rv;
}

static Handle<Object> RecordReplayGetPossibleBreakpoints(Isolate* isolate,
                                                         Handle<Object> params) {
  std::vector<std::vector<int>> lineColumns;
  int numLines = 0;

  ForEachInstrumentationOpInRange(isolate, params,
     [&](Handle<Script> script, int bytecode_offset,
         const std::string& function_id, int line, int column) {
    while ((size_t)line >= lineColumns.size()) {
      lineColumns.emplace_back();
    }
    if (!lineColumns[line].size()) {
      numLines++;
    }
    lineColumns[line].push_back(column);
  });

  Handle<FixedArray> lineLocations = isolate->factory()->NewFixedArray(numLines);
  int lineLocationsIndex = 0;
  for (size_t line = 0; line < lineColumns.size(); line++) {
    const std::vector<int>& baseColumns = lineColumns[line];
    if (!baseColumns.size()) {
      continue;
    }

    Handle<FixedArray> columns = isolate->factory()->NewFixedArray((int)baseColumns.size());
    for (int i = 0; i < (int)baseColumns.size(); i++) {
      columns->set(i, Smi::FromInt(baseColumns[i]));
    }
    Handle<JSArray> columnsArray = isolate->factory()->NewJSArrayWithElements(columns);

    Handle<JSObject> lineObj = NewPlainObject(isolate);
    SetProperty(isolate, lineObj, "line", line);
    SetProperty(isolate, lineObj, "columns", columnsArray);
    lineLocations->set(lineLocationsIndex++, *lineObj);
  }
  DCHECK(lineLocationsIndex == numLines);

  Handle<JSArray> lineLocationsArray =
    isolate->factory()->NewJSArrayWithElements(lineLocations);

  Handle<JSObject> rv = NewPlainObject(isolate);
  SetProperty(isolate, rv, "lineLocations", lineLocationsArray);
  return rv;
}

static SharedFunctionInfo GetSharedFunctionInfoById(Isolate* isolate,
                                                    Handle<Script> script,
                                                    int startPosition) {
  SharedFunctionInfo::ScriptIterator iterator(isolate, *script);
  for (SharedFunctionInfo info = iterator.Next(); !info.is_null();
       info = iterator.Next()) {
    if (info.StartPosition() == startPosition) {
      return info;
    }
  }
  SharedFunctionInfo empty;
  return empty;
}

extern void ParseRecordReplayFunctionId(const std::string& function_id,
                                        int* script_id, int* source_position);

static void ParseRecordReplayFunctionIdFromParams(Isolate* isolate,
                                                  Handle<Object> params,
                                                  std::string* function_id,
                                                  int* script_id,
                                                  int* source_position) {
  Handle<Object> function_id_raw = GetProperty(isolate, params, "functionId");
  std::unique_ptr<char[]> function_id_chars =
      String::cast(*function_id_raw).ToCString();
  *function_id = function_id_chars.get();
  ParseRecordReplayFunctionId(*function_id, script_id, source_position);
}

v8::Local<v8::Object> RecordReplayGetBytecode(v8::Isolate* isolate_,
                                                     v8::Local<v8::Object> paramsObj) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(isolate_);
  int script_id, function_source_position;
  std::string function_id;
  Handle<Object> params = Utils::OpenHandle(*paramsObj);
  ParseRecordReplayFunctionIdFromParams(isolate, params, &function_id,
                                        &script_id, &function_source_position);
  MaybeHandle<Script> maybe_script = MaybeGetScript(isolate, script_id);
  Handle<JSObject> rv = NewPlainObject(isolate);

  if (!maybe_script.is_null()) {
    Handle<Script> script = maybe_script.ToHandleChecked();
    SetProperty(isolate, rv, "sourceId", script_id);

    SharedFunctionInfo info =
        GetSharedFunctionInfoById(isolate, script, function_source_position);
    if (!info.is_null()) {
      SetProperty(isolate, rv, "name", SharedFunctionInfo::DebugName(handle(info, isolate)));
      SetProperty(isolate, rv, "debuggable", info.IsSubjectToDebugging());
      SetProperty(isolate, rv, "compiled", info.is_compiled());
      SetProperty(isolate, rv, "hasBytecode", info.HasBytecodeArray());

      if (info.IsSubjectToDebugging() && info.is_compiled() &&
          info.HasBytecodeArray()) {
        // Get Bytecode.
        // (based on InterpreterCompilationJob::DoFinalizeJobImpl)
        BytecodeArray bytecode = info.GetBytecodeArray(isolate);
        std::ostringstream bytecodeResult;
        bytecode.Disassemble(bytecodeResult);

        SetProperty(isolate, rv, "bytecode", bytecodeResult.str().c_str());
        SetProperty(isolate, rv, "bytecodeLength", bytecode.length());
      }
    }
  }

  return v8::Utils::ToLocal(rv);
}

void RecordReplayGetPossibleBreakpointsCallback(const char* script_id_str) {
  Isolate* isolate = Isolate::Current();
  base::Optional<SaveAndSwitchContext> ssc;
  EnsureIsolateContext(isolate, ssc);

  HandleScope scope(isolate);

  int script_id = atoi(script_id_str);
  MaybeHandle<Script> maybe_script = MaybeGetScript(isolate, script_id);

  if (maybe_script.is_null()) {
    return;
  }

  Handle<Script> script = maybe_script.ToHandleChecked();

  ForEachInstrumentationOp(isolate, script, [&](Handle<SharedFunctionInfo> shared,
                                                int instrumentation_index) {
    if (strcmp(InstrumentationSiteKind(instrumentation_index), "breakpoint")) {
      return;
    }

    int line, column;
    GetInstrumentationSiteLocation(script, instrumentation_index, &line, &column);

    int function_index = InstrumentationSiteFunctionIndex(instrumentation_index);

    std::string function_id = GetRecordReplayFunctionId(shared);
    RecordReplayAddPossibleBreakpoint(line, column, function_id.c_str(), function_index);
  });
}

static Handle<Object> RecordReplayConvertFunctionOffsetToLocation(
    Isolate* isolate, Handle<Object> params) {
  int script_id, function_source_position;
  std::string function_id;
  ParseRecordReplayFunctionIdFromParams(isolate, params, &function_id,
                                        &script_id, &function_source_position);
  Handle<Script> script = GetScript(isolate, script_id);
  Handle<Object> offset_raw = GetProperty(isolate, params, "offset");

  // The offset may or may not be present. If the offset is present, use it as the
  // instrumentation site to get the source position.
  int line = 0, column = 0;
  if (offset_raw->IsNumber()) {
    int bytecode_offset = offset_raw->Number();

    std::string key = BreakpointPositionKey(function_id, bytecode_offset);
    if (!gBreakpointPositions) {
      GenerateBreakpointInfo(isolate, script);
    }
    auto iter = gBreakpointPositions->find(key);
    if (iter == gBreakpointPositions->end()) {
      GenerateBreakpointInfo(isolate, script);
      iter = gBreakpointPositions->find(key);
    }

    if (iter != gBreakpointPositions->end()) {
      line = iter->second.line_;
      column = iter->second.column_;
    } else {
      recordreplay::Diagnostic("Unknown offset %s %d for RecordReplayConvertFunctionOffsetToLocation",
                                function_id.c_str(), bytecode_offset);
    }
  }

  // If there wasn't an offset or an unexpected unknown offset was encountered,
  // fallback to the position of the function itself. Note that if we successfully
  // looked up a breakpoint position the line will not be zero because it is 1-indexed,
  // see GetInstrumentationSiteLocation.
  if (!line) {
    Script::PositionInfo info;
    Script::GetPositionInfo(script, function_source_position, &info, Script::WITH_OFFSET);

    // Use 1-indexed lines instead of 0-indexed.
    line = info.line + 1;
    column = info.column;
  }

  Handle<JSObject> location = NewPlainObject(isolate);
  SetProperty(isolate, location, "sourceId", GetProtocolSourceId(isolate, script));
  SetProperty(isolate, location, "line", line);
  SetProperty(isolate, location, "column", column);

  Handle<JSObject> rv = NewPlainObject(isolate);
  SetProperty(isolate, rv, "location", location);
  return rv;
}

bool RecordReplayHasRegisteredScript(Script script);

static Handle<Object> RecordReplayCountStackFrames(Isolate* isolate,
                                                   Handle<Object> params) {
  // This is handled in C++ instead of via a protocol JS handler for efficiency.
  // Counting the stack frames is a common operation when there are many
  // exception unwinds and so forth.
  size_t count = 0;
  for (StackFrameIterator it(isolate); !it.done(); it.Advance()) {
    StackFrame* frame = it.frame();
    if (!frame->is_java_script()) {
      continue;
    }
    std::vector<FrameSummary> frames;
    CommonFrame::cast(frame)->Summarize(&frames);

    // We don't strictly need to iterate the frames in reverse order, but it
    // helps when logging the stack contents for debugging.
    for (int i = (int)frames.size() - 1; i >= 0; i--) {
      const auto& summary = frames[i];
      CHECK(summary.IsJavaScript());
      const auto& js = summary.AsJavaScript();

      Handle<SharedFunctionInfo> shared(js.function()->shared(), isolate);

      // See GetStackLocation.
      if (!shared->StartPosition() && !shared->EndPosition()) {
        continue;
      }

      Handle<Script> script(Script::cast(shared->script()), isolate);
      if (script->id() && RecordReplayHasRegisteredScript(*script)) {
        count++;
      }
    }
  }

  Handle<JSObject> rv = NewPlainObject(isolate);
  SetProperty(isolate, rv, "count", count);
  return rv;
}

static Handle<Object> RecordReplayGetFunctionsInRange(Isolate* isolate,
                                                      Handle<Object> params) {
  std::set<std::string> functions;
  ForEachInstrumentationOpInRange(isolate, params,
     [&](Handle<Script> script, int bytecode_offset,
         const std::string& function_id, int line, int column) {
    functions.insert(function_id);
  });

  Handle<FixedArray> functionsArray = isolate->factory()->NewFixedArray((int)functions.size());

  int index = 0;
  for (const std::string& function_id : functions) {
    Handle<String> str = CStringToHandle(isolate, function_id.c_str());
    functionsArray->set(index++, *str);
  }
  CHECK(index == (int)functions.size());

  Handle<JSArray> functionsJSArray =
    isolate->factory()->NewJSArrayWithElements(functionsArray);

  Handle<JSObject> rv = NewPlainObject(isolate);
  SetProperty(isolate, rv, "functions", functionsJSArray);
  return rv;
}

struct HTMLParseData {
  void* token_;
  std::string url_;
  std::string contents_;
  HTMLParseData(void* token, const char* url) : token_(token), url_(url) {}
};
typedef std::vector<HTMLParseData> HTMLParseDataVector;
static HTMLParseDataVector* gHTMLParses;

extern void RecordReplayAddHTMLParse(const char* url);

extern "C" void V8RecordReplayHTMLParseStart(void* token, const char* url) {
  if (!gHTMLParses) {
    gHTMLParses = new HTMLParseDataVector();
  }
  RecordReplayAddHTMLParse(url);
  gHTMLParses->emplace_back(token, url);
}

extern "C" void V8RecordReplayHTMLParseFinish(void* token) {
  if (gHTMLParses) {
    for (HTMLParseData& data : *gHTMLParses) {
      if (data.token_ == token) {
        data.token_ = nullptr;
      }
    }
  }
}

extern "C" void V8RecordReplayHTMLParseAddData(void* token, const char* text) {
  if (gHTMLParses) {
    for (HTMLParseData& data : *gHTMLParses) {
      if (data.token_ == token) {
        data.contents_ += text;
        break;
      }
    }
  }
}

static Handle<Object> RecordReplayGetHTMLSource(Isolate* isolate, Handle<Object> params) {
  Handle<Object> url_raw = GetProperty(isolate, params, "url");
  std::unique_ptr<char[]> url_chars = String::cast(*url_raw).ToCString();

  std::string contents;
  if (gHTMLParses) {
    for (const HTMLParseData& data : *gHTMLParses) {
      if (data.url_ == url_chars.get()) {
        contents = data.contents_;
        break;
      }
    }
  }

  Handle<JSObject> rv = NewPlainObject(isolate);
  SetProperty(isolate, rv, "contents", contents.c_str());
  return rv;
}

extern int RecordReplayCurrentGeneratorIdRaw();

static Handle<Object> RecordReplayCurrentGeneratorId(Isolate* isolate, Handle<Object> params) {
  Handle<JSObject> rv = NewPlainObject(isolate);
  int id = RecordReplayCurrentGeneratorIdRaw();
  if (id) {
    SetProperty(isolate, rv, "id", id);
  }
  return rv;
}

// TODO: Use |SNPrintF| from src/base/strings.h instead.
static std::string StringPrintf(const char* format, ...) {
  char buf[4096];
  buf[sizeof(buf) - 1] = 0;
  va_list ap;
  va_start(ap, format);
  vsnprintf(buf, sizeof(buf) - 1, format, ap);
  va_end(ap);
  return std::string(buf);
}

// When assertions are used we assign an ID to each object that is ever
// encountered in one, so that we can determine whether consistent objects
// are used when replaying.
struct ContextObjectIdMap {
  v8::Global<v8::Context> context_;
  v8::Global<v8::Value> object_ids_;
};
typedef std::vector<ContextObjectIdMap> ContextObjectIdMapVector;
static ContextObjectIdMapVector* gRecordReplayObjectIds;

static Local<v8::Value> GetObjectIdMapForContext(v8::Isolate* v8_isolate, Local<v8::Context> cx) {
  Isolate* isolate = (Isolate*)v8_isolate;

  if (!gRecordReplayObjectIds) {
    gRecordReplayObjectIds = new ContextObjectIdMapVector();
  }

  for (const auto& entry : *gRecordReplayObjectIds) {
    if (entry.context_ == cx) {
      return entry.object_ids_.Get(v8_isolate);
    }
  }

  Handle<Object> object_ids = isolate->factory()->NewJSWeakMap();

  ContextObjectIdMap new_entry;
  new_entry.context_.Reset(v8_isolate, cx);
  new_entry.object_ids_.Reset(v8_isolate, v8::Utils::ToLocal(object_ids));
  gRecordReplayObjectIds->push_back(std::move(new_entry));
  return gRecordReplayObjectIds->back().object_ids_.Get(v8_isolate);
}

extern bool gRecordReplayAssertTrackedObjects;
extern int (*gGetAPIObjectIdCallback)(v8::Local<v8::Object> object);

static int gNextObjectId = 1;

int RecordReplayObjectId(v8::Isolate* v8_isolate, v8::Local<v8::Context> cx,
                         v8::Local<v8::Value> v8_object, bool allow_create) {
  CHECK(IsMainThread());

  if (!v8_object->IsObject()) {
    return 0;
  }

  if (gGetAPIObjectIdCallback) {
    // Check for Blink objects.
    int api_id = gGetAPIObjectIdCallback(v8_object.As<v8::Object>());
    if (api_id) {
      return api_id;
    }
  }

  Isolate* isolate = (Isolate*)v8_isolate;
  Handle<Object> object = Utils::OpenHandle(*v8_object);

  // Look through all weak maps we've created, the object might not be associated
  // with the current context.
  if (gRecordReplayObjectIds) {
    for (const auto& entry : *gRecordReplayObjectIds) {
      Local<v8::Value> object_ids_val = entry.object_ids_.Get(v8_isolate);
      Handle<JSWeakMap> object_ids = Handle<JSWeakMap>::cast(Utils::OpenHandle(*object_ids_val));

      Handle<Object> existing(EphemeronHashTable::cast(object_ids->table()).Lookup(object), isolate);
      if (!existing->IsTheHole(isolate)) {
        v8::Local<v8::Value> id_value = v8::Utils::ToLocal(existing);
        if (id_value->IsInt32()) {
          int id = id_value.As<v8::Int32>()->Value();
          if (gRecordReplayAssertTrackedObjects) {
            recordreplay::AssertMaybeEventsDisallowed("JS ReuseObjectId %d", id);
          }
          return id;
        }
      }
    }
  }

  if (!allow_create) {
    return 0;
  }

  int id = gNextObjectId++;

  if (gRecordReplayAssertTrackedObjects) {
    recordreplay::AssertMaybeEventsDisallowed("JS NewObjectId %d", id);
  }

  Local<Value> id_value = v8::Integer::New(v8_isolate, id);

  int32_t hash = object->GetOrCreateHash(isolate).value();

  Local<v8::Value> object_ids_val = GetObjectIdMapForContext(v8_isolate, cx);
  Handle<JSWeakMap> object_ids = Handle<JSWeakMap>::cast(Utils::OpenHandle(*object_ids_val));
  JSWeakCollection::Set(object_ids, object, Utils::OpenHandle(*id_value), hash);

  return id;
}

static bool gTrackObjects = false;

// Called by the recorder when we need to track persistent IDs for as many objects
// as we are able to. Currently this is enabled while replaying via the
// enablePersistentIDs experimental setting.
void TrackObjectsCallback(bool track_objects) {
  CHECK(recordreplay::IsReplaying());
  gTrackObjects = track_objects;
}

// Whether to keep track of 'this' objects being assigned a property.
bool RecordReplayTrackThisObjectAssignment(const std::string& property) {
  // If we've been told to track objects then all 'this' objects which are
  // assigned a property will be tracked.
  if (gRecordReplayAssertTrackedObjects || gTrackObjects) {
    return true;
  }

  // By default we only track objects which might be React fibers. These will
  // have an "alternate" property assigned to in the constructor. Tracking objects
  // is only needed when replaying.
  if (recordreplay::IsReplaying() && property == "alternate") {
    return true;
  }

  return false;
}

// Print a message if an object does not have a persistent ID. For use in testing.
void RecordReplayConfirmObjectHasId(v8::Isolate* isolate, v8::Local<v8::Context> cx,
                                    v8::Local<v8::Value> object) {
  int id = RecordReplayObjectId(isolate, cx, object, /* allow_create */ false);
  if (!id) {
    recordreplay::Print("RecordReplayConfirmObjectHasId unexpected missing persistent ID");
  }
}

inline int HashBytes(const void* aPtr, size_t aSize) {
  int hash = 0;
  uint8_t* ptr = (uint8_t*)aPtr;
  for (size_t i = 0; i < aSize; i++) {
    hash = (((hash << 5) - hash) + ptr[i]) | 0;
  }
  return hash;
}

int (*gGetAPIObjectIdCallback)(v8::Local<v8::Object> object);

extern "C" void V8RecordReplaySetAPIObjectIdCallback(int (*callback)(v8::Local<v8::Object>)) {
  gGetAPIObjectIdCallback = callback;
}

// Get a string that can be included in recording assertions.
static std::string ToReadableString(const char* str) {
  std::ostringstream o;
  for (; *str; str++) {
    if ((int)*str >= 32 && (int)*str <= 126) {
      o << *str;
    } else {
      o << "\\" << (int)*str;
    }
  }
  return o.str();
}

// Get a string describing a value which can be used in assertions.
// Only basic information about the value is obtained, to keep things fast.
std::string RecordReplayBasicValueContents(Handle<Object> value) {
  if (value->IsNumber()) {
    double num = value->Number();
    if (std::isnan(num)) {
      return "NaN";
    }
    int num2 = num;
    if (num2 != num) {
      num2 = -1;
    }
    return StringPrintf("Number %d %llu", num2, *(uint64_t*)&num);
  }

  if (value->IsBoolean()) {
    return StringPrintf("Boolean %d", value->IsTrue());
  }

  if (value->IsUndefined()) {
    return "Undefined";
  }

  if (value->IsNull()) {
    return "Null";
  }

  if (value->IsString()) {
    String str = String::cast(*value);
    if (str.length() <= 200) {
      std::unique_ptr<char[]> name = str.ToCString();
      std::string readable_name = ToReadableString(name.get());
      return StringPrintf("String %s", readable_name.c_str());
    }
    return StringPrintf("LongString %d", str.length());
  }

  if (value->IsJSObject()) {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Local<v8::Context> cx = isolate->GetCurrentContext();
    int object_id = RecordReplayObjectId(isolate, cx, v8::Utils::ToLocal(value),
                                         /* allow_create */ true);

    InstanceType type = JSObject::cast(*value).map().instance_type();
    const char* typeStr;
    switch (type) {
#define STRINGIFY_TYPE(TYPE) case TYPE: typeStr = #TYPE; break;
    INSTANCE_TYPE_LIST(STRINGIFY_TYPE)
#undef STRINGIFY_TYPE
    default:
      typeStr = "<unknown>";
    }
    if (!strcmp(typeStr, "JS_DATE_TYPE")) {
      JSDate date = JSDate::cast(*value);
      double time = date.value().Number();
      return StringPrintf("Date %d %.2f", object_id, time);
    }
    if (!strcmp(typeStr, "JS_TYPED_ARRAY_TYPE")) {
      v8::Local<v8::Value> obj = v8::Utils::ToLocal(value);
      v8::Local<v8::TypedArray> tarr = obj.As<v8::TypedArray>();
      char buf[50];
      size_t written = tarr->CopyContents(buf, sizeof(buf));
      int hash = HashBytes(buf, written);
      return StringPrintf("TypedArray %d %lu %d", object_id, tarr->ByteLength(), hash);
    }
    return StringPrintf("Object %d %s", object_id, typeStr);
  }

  if (value->IsJSProxy()) {
    return "Proxy";
  }

  return "Unknown";
}

// Information in the dependency graph for a JS promise.
struct PromiseDependencyGraphData {
  // Graph node ID for the point the promise was created.
  int new_node_id = 0;

  // Graph node ID for the point the promise was settled.
  int settled_node_id = 0;
};

typedef std::unordered_map<int32_t, PromiseDependencyGraphData> PromiseDependencyGraphDataMap;
static PromiseDependencyGraphDataMap* gPromiseDependencyGraphDataMap;

static PromiseDependencyGraphData&
GetOrCreatePromiseDependencyGraphData(Isolate* isolate, Handle<Object> promise) {
  v8::Isolate* v8_isolate = (v8::Isolate*) isolate;
  int promise_object_id =
    RecordReplayObjectId(v8_isolate, v8_isolate->GetCurrentContext(),
                         v8::Utils::ToLocal(promise), /* allow_create */ true);

  CHECK(IsMainThread());
  if (!gPromiseDependencyGraphDataMap) {
    gPromiseDependencyGraphDataMap = new PromiseDependencyGraphDataMap();
  }
  auto iter = gPromiseDependencyGraphDataMap->find(promise_object_id);
  if (iter == gPromiseDependencyGraphDataMap->end()) {
    (*gPromiseDependencyGraphDataMap)[promise_object_id] = PromiseDependencyGraphData();
    iter = gPromiseDependencyGraphDataMap->find(promise_object_id);
  }
  return iter->second;
}

extern bool gRecordReplayEnableDependencyGraph;

bool RecordReplayShouldCallOnPromiseHook() {
  // The promise hook is normally only used when replaying, but can assign
  // persistent IDs to objects so needs to be called while recording if we are
  // asserting on these.
  return gRecordReplayEnableDependencyGraph
      && (recordreplay::IsReplaying() || gRecordReplayAssertTrackedObjects)
      && IsMainThread();
}

void RecordReplayOnPromiseHook(Isolate* isolate, PromiseHookType type,
                               Handle<JSPromise> promise, Handle<Object> parent) {
  CHECK(RecordReplayShouldCallOnPromiseHook());

  if (!gRecordReplayEnableDependencyGraph) {
    return;
  }

  PromiseDependencyGraphData& data =
    GetOrCreatePromiseDependencyGraphData(isolate, promise);

  switch (type) {
    case PromiseHookType::kInit: {
      CHECK(!data.new_node_id);
      data.new_node_id = recordreplay::NewDependencyGraphNode("{\"kind\":\"newPromise\"}");
      if (!parent->IsUndefined()) {
        PromiseDependencyGraphData& parent_data =
          GetOrCreatePromiseDependencyGraphData(isolate, parent);
        if (parent_data.new_node_id) {
          recordreplay::AddDependencyGraphEdge(parent_data.new_node_id, data.new_node_id,
                                               "{\"kind\":\"parentPromise\"}");
        }
      }
      break;
    }
    case PromiseHookType::kResolve: {
      if (!data.new_node_id || data.settled_node_id) {
        break;
      }
      data.settled_node_id =
        recordreplay::NewDependencyGraphNode("{\"kind\":\"promiseSettled\"}");
      recordreplay::AddDependencyGraphEdge(data.new_node_id, data.settled_node_id,
                                           "{\"kind\":\"basePromise\"}");
      break;
    }
    case PromiseHookType::kBefore: {
      int node_id = data.settled_node_id ? data.settled_node_id : data.new_node_id;
      recordreplay::BeginDependencyExecution(node_id);
      break;
    }
    case PromiseHookType::kAfter: {
      recordreplay::EndDependencyExecution();
      break;
    }
  }
}

void FunctionCallbackRecordReplayGetScriptSource(const FunctionCallbackInfo<Value>& callArgs) {
  CHECK(recordreplay::IsRecordingOrReplaying());
  CHECK(IsMainThread());
  CHECK(callArgs.Length() == 1);
  CHECK(callArgs[0]->IsString());

  i::Handle<i::Object> id_obj = Utils::OpenHandle(*callArgs[0]);

  std::unique_ptr<char[]> script_id_text = i::String::cast(*id_obj).ToCString();
  int script_id = atoi(script_id_text.get());

  i::Isolate* isolate = (i::Isolate*)callArgs.GetIsolate();

  i::Handle<i::Script> script = i::GetScript(isolate, script_id);
  i::Handle<i::String> source(i::String::cast(script->source()), isolate);

  Local<Value> source_val = v8::Utils::ToLocal(source);
  callArgs.GetReturnValue().Set(source_val);
}

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

/** ###########################################################################
 * CommandCallback
 * ##########################################################################*/

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
    recordreplay::Crash("CommandCallback Parse %s failed", params);
  }
  i::Handle<i::Object> paramsObj = maybeParams.ToHandleChecked();

  i::Handle<i::Object> result;
  for (const InternalCommandCallback& cb : gInternalCommandCallbacks) {
    if (!strcmp(cb.mCommand, command)) {
      i::MaybeHandle<i::Object> rv = cb.mCallback(isolate, paramsObj);
      if (rv.is_null()) {
        recordreplay::Crash("CommandCallback internal command %s failed", command);
      }
      
      result = rv.ToHandleChecked();
    }
  }
  if (result.is_null()) {
    // Handle command with the JS command handler.
    replayio::ReplayRootContext* root = replayio::RecordReplayGetRootContext();
    CHECK(root);

    constexpr int NCallArgs = 2;
    Local<Value> callArgs[NCallArgs] = {
      replayio::CStringToLocal((v8::Isolate*)isolate, command),
      Utils::ToLocal(paramsObj)
    };
    Local<Value> local_result = root->EmitReplayEvent(
      "command",
      NCallArgs,
      callArgs,
      "emitWithResult"
    );

    result = Utils::OpenHandle(*local_result);
  }
  
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

/** ###########################################################################
 * {@link ClearPauseDataCallback}
 * ##########################################################################*/

void ClearPauseDataCallback() {
  CHECK(IsMainThread());
  AutoMarkReplayCode amrc;
  replayio::AutoDisallowEvents disallow("ClearPauseDataCallback");

  // TODO: Call this on all live root contexts.
  replayio::ReplayRootContext* rootContext = replayio::RecordReplayGetRootContext();
  if (!rootContext) {
    return;
  }

  i::Isolate* isolate = i::Isolate::Current();
  base::Optional<i::SaveAndSwitchContext> ssc;
  EnsureIsolateContext(isolate, ssc);

  rootContext->EmitReplayEvent("clearPauseData");
}

}  // namespace internal
}  // namespace v8