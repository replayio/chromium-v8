/** Copyright 2023 Record Replay Inc. */

#ifndef V8_REPLAY_REPLAY_UTIL_H_
#define V8_REPLAY_REPLAY_UTIL_H_

#include <string>

#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/objects/script.h"
#include "src/objects/string.h"
#include "src/replay/replay-common.h"

namespace recordreplay {
/**
 * Normalized script name with edge case handling.
 */
inline std::string GetScriptName(
    v8::internal::Handle<v8::internal::Script> script) {
  return script->name().IsString()
             ? v8::internal::String::cast(script->name()).ToCString().get()
             : "(anonymous script)";
}

  /**
   * Get a script location string of the format:
   * |"<script-name>:<line>:<col>"|
   */
  std::string GetScriptLocationString(int script_id, int start_position);

  /**
   * Get a script location string of the format:
   * |"PC=<pc> scriptId=<scriptId> @<script-name>:<line>:<col> stack=<js-stack>"|
   */
  std::string GetCurrentLocationStringExtended(int script_id = -1, int start_position = 0);

  }  // namespace recordreplay

#endif
