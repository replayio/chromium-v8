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
  // record/replay: The original hook registered the underlying pthread mutex
  // with the replay backend via V8RecordReplayAddOrderedPthreadMutex() so that
  // lock/unlock ordering could be made deterministic. In this V8 the platform
  // mutex is backed by absl::Mutex (native_handle_), which does not expose a
  // pthread_mutex_t and is not implemented in terms of one, so there is no
  // valid pthread_mutex_t* to register. Reinterpreting &native_handle_ as a
  // pthread_mutex_t* would feed the backend a non-pthread object and cause
  // divergence/crashes, so the registration is intentionally skipped here.
  // The constructor still accepts |ordered_name| to stay ABI/source compatible
  // with the ported header and call sites that pass a name.
  USE(ordered_name);
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
