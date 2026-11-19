// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/mutex.h"

#include "src/base/platform/platform.h"

namespace v8 {
namespace base {

RecursiveMutex::~RecursiveMutex() {
  DCHECK_EQ(0, level_);
}

void RecursiveMutex::Lock() {
  int own_id = v8::base::OS::GetCurrentThreadId();
  if (thread_id_ == own_id) {
    level_++;
    return;
  }
  mutex_.Lock();
  DCHECK_EQ(0, level_);
  thread_id_ = own_id;
  level_ = 1;
}

void RecursiveMutex::Unlock() {
#ifdef DEBUG
  int own_id = v8::base::OS::GetCurrentThreadId();
  CHECK_EQ(thread_id_, own_id);
#endif
  if ((--level_) == 0) {
    thread_id_ = 0;
    mutex_.Unlock();
  }
}

bool RecursiveMutex::TryLock() {
  int own_id = v8::base::OS::GetCurrentThreadId();
  if (thread_id_ == own_id) {
    level_++;
    return true;
  }
  if (mutex_.TryLock()) {
    DCHECK_EQ(0, level_);
    thread_id_ = own_id;
    level_ = 1;
    return true;
  }
  return false;
}

Mutex::Mutex(const char* ordered_name) {
  // An ordered name is inert here, and deliberately not an error. The driver
  // can order the acquisitions of a lock it was handed as a pthread_mutex_t
  // (RecordReplayAddOrderedPthreadMutex), which is what the 10.8 fork's POSIX
  // constructor did with this name. Upstream now builds Mutex on absl::Mutex,
  // which spins and then blocks in syscall(SYS_futex, ...), a call the driver
  // passes through and records nothing of. So there is nothing to register: no
  // V8 base lock has an acquisition order in a recording, and a contended one
  // busy-waits while replaying, because the linker answers an untimed
  // FUTEX_WAIT with EINTR.
  //
  // The capability is left dormant rather than removed, because nothing in V8
  // or in the embedders passes a name today and restoring it would need no
  // caller to change: it would mean holding a recordreplay::CreateOrderedLock()
  // id here, as zero when AreEventsDisallowed(), since a LazyMutex can be
  // constructed anywhere, and taking AutoOrderedLock in Lock, Unlock, TryLock
  // and around a condition variable's reacquire. Beware that an audit which
  // compares lock names against 10.8 reads these locks as ordered while
  // nothing orders them.
  (void)ordered_name;
#ifdef DEBUG
  level_ = 0;
#endif
}

Mutex::~Mutex() { DCHECK_EQ(0, level_); }

void Mutex::Lock() ABSL_NO_THREAD_SAFETY_ANALYSIS {
  native_handle_.lock();
  AssertUnheldAndMark();
}

void Mutex::Unlock() ABSL_NO_THREAD_SAFETY_ANALYSIS {
  AssertHeldAndUnmark();
  native_handle_.unlock();
}

bool Mutex::TryLock() ABSL_NO_THREAD_SAFETY_ANALYSIS {
  if (!native_handle_.try_lock()) return false;
  AssertUnheldAndMark();
  return true;
}

}  // namespace base
}  // namespace v8
