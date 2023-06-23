/* Copyright 2023 Record Replay Inc. */

#include "./replay-util.h"

#include "src/api/api-inl.h"


namespace v8 {
namespace internal {
extern uint64_t* gProgressCounter;
extern Handle<Script> GetScript(Isolate* isolate, int script_id);
}
}

using namespace v8::internal;

namespace recordreplay {
std::string GetScriptLocationString(int script_id, int start_position) {
  Isolate* isolate = Isolate::Current();
  Handle<Script> script = GetScript(isolate, script_id);
  std::string script_name = GetScriptName(script);
  if (script_name.empty()) {
    return "(no script)";
  }

  Script::PositionInfo info;
  Script::GetPositionInfo(script, start_position, &info, Script::WITH_OFFSET);

  std::ostringstream os;
  os << script_id << ":" << script_name << ":" << info.line + 1 << ":"
     << info.column;
  return os.str();
}

std::string GetCurrentLocationStringExtended(int script_id,
                                             int start_position) {
  std::ostringstream os;

  os << "PC=" << *gProgressCounter;

  if (script_id > 0) {
    std::string loc = GetScriptLocationString(script_id, start_position);
    os << " scriptId=" << script_id << " @" << loc.c_str();
  }
  
  Isolate* isolate = Isolate::Current();
  HandleScope scope(isolate);
  
  os << " stack=";
  isolate->PrintCurrentStackTrace(os);

  return os.str();
}
}
