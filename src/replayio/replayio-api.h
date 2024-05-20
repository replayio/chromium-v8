// Copyright (c) 2024 Record Replay Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains all kinds of utilities used in Replay code.

#ifndef V8_REPLAY_REPLAYIO_API_H
#define V8_REPLAY_REPLAYIO_API_H

#include "include/replayio.h"
#include <unordered_set>

namespace v8 {
namespace replayio {
using ScriptIdMap = std::unordered_map<int, Eternal<Value>>;
using ScriptIdSet = std::unordered_set<int>;



}  // namespace replayio
}  // namespace v8

#endif  // V8_REPLAY_REPLAYIO_API_H