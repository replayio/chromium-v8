// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is a clone of "base/optional.h" in chromium.
// Keep in sync, especially when fixing bugs.
// Copyright 2017 the V8 project authors. All rights reserved.

#ifndef V8_BASE_REPLAYIO_H
#define V8_BASE_REPLAYIO_H

#include "include/replayio.h"

#include <optional>

namespace v8 {
namespace replayio {

inline bool CheckReplayOwned() {
  return v8::recordreplay::IsInReplayCode();
}

// Whether user JS (an accessor, toString, Symbol.toPrimitive, a callback the
// page installed) must not run now: Replay's own code is running, so events
// are disallowed, and the replay hasn't diverged into a pause, where user JS
// may run because nothing it does reaches the replayed execution.
//
// Replay's own code doesn't run the same way when recording and replaying.
// Replay's inspector session is installed only when replaying (unless the
// replay-only-command-handling feature is disabled), and the driver gathers
// console message contents, previews and frames (OnConsoleMessage) only when
// replaying. User JS run from there would change the page's state where the
// recording didn't, so Replay's session has to behave as if it weren't
// attached: describing a value for it skips anything that would run user JS.
// Sessions a user or the page attached (DevTools, Playwright) run with events
// allowed, so they run user JS like upstream does, recorded and replayed.
//
// The alternative would be to embrace the session instead: install it on both
// sides and record its work, with events allowed, for everything it does at
// the same points when recording and replaying (describing console arguments,
// gathering console message data). The user JS it runs would then replay like
// any other. The cost is that every recording shows the page as if DevTools
// were open: its accessors and toString run where they don't without it
// (anti-bot checks like Cloudflare Turnstile detect exactly that), and every
// console message pays for the descriptions when recording. Work queried
// later, on demand, would still have to run in a pause.
inline bool MustNotRunUserJS() {
  return v8::recordreplay::AreEventsDisallowed() &&
         !v8::recordreplay::HasDivergedFromRecording();
}

// Whether reading |name| on |object| runs none of the page's code, so that a
// description may read it where MustNotRunUserJS() holds. The description's gate
// is otherwise closed to every builtin getter, so this says yes only for reads
// whose cost it knows, and no for everything else.
//
// The one it knows is an Error's own "stack", whose getter formats the stack
// with the page's Error.prepareStackTrace. Yes where the stack is already
// stored, which the getter hands back as it is, where the property holds
// something the getter returns untouched, and where the page formats stacks
// V8's way, so V8 builds the string itself and the page's own read would get
// that same string. No while the page's formatter would run.
//
// The property has to be |object|'s own accessor whose getter is one of V8's,
// so a page getter shadowing an inherited one, a proxy trap, an interceptor and
// an access check all end the lookup elsewhere.
//
// Defined in src/replay/replayio.cc.
bool ReadingIsFreeOfUserJS(v8::Local<v8::Object> object,
                           v8::Local<v8::Name> name);

// Marks nested code as replay-owned (IsInReplayCode). Independent of
// AutoMaybeDisallowEvents; divergent replay-owned paths use both.
struct AutoMaybeMarkReplayCode {
  explicit AutoMaybeMarkReplayCode(bool markReplayCode) {
    if (markReplayCode) {
      mark.emplace();
    }
  }

 private:
  std::optional<v8::replayio::AutoMarkReplayCode> mark;
};

struct AutoMaybeDisallowEvents {
  AutoMaybeDisallowEvents(bool disallowEvents, v8::Isolate* isolate,
                          const char* label) {
    if (disallowEvents) {
      disallow.emplace(label, isolate);
    }
  }

 private:
  std::optional<v8::replayio::AutoDisallowEvents> disallow;
};

// Mark+Disallow for replay-owned inspector object work sites.
struct AutoMaybeReplayOwned {
  AutoMaybeReplayOwned(bool owned, v8::Isolate* isolate, const char* label)
      : mark(owned), disallow(owned, isolate, label) {}

 private:
  AutoMaybeMarkReplayCode mark;
  AutoMaybeDisallowEvents disallow;
};

}  // namespace replayio
}  // namespace v8

#endif  // V8_BASE_REPLAYIO_H
