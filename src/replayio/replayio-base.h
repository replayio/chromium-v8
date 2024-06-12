// Copyright (c) 2024 Record Replay Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Use this header file to access 

#ifndef V8_REPLAY_REPLAYIO_BASE_H
#define V8_REPLAY_REPLAYIO_BASE_H

#include "include/replayio.h"
#include "src/base/optional.h"

namespace v8 {
namespace replayio {

struct AutoMaybeDisallowEvents {
  AutoMaybeDisallowEvents(bool disallowEvents, const char* label) {
    if (disallowEvents) {
      disallow.emplace(label);
    }
  }
  
private:
  v8::base::Optional<v8::replayio::AutoDisallowEvents> disallow;
};

}  // namespace replayio
}  // namespace v8

#endif  // V8_REPLAY_REPLAYIO_BASE_H
