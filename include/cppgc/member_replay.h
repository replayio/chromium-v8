// Copyright (c) 2023 Record Replay Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MEMBER_REPLAY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MEMBER_REPLAY_H_

#include "base/check_op.h"
#include "base/record_replay.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/thread_state_storage.h"
#include "third_party/blink/renderer/platform/heap/write_barrier.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/construct_traits.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"
#include "v8/include/cppgc/member.h"

namespace cppgc {
namespace internal {
template <typename T>

#define ReplayLeakWeak recordreplay::IsRecordingOrReplaying("avoid-weak-pointers")

/**
 *
 */
class ReplayWeakMember : public GarbageCollectedMixin {
 public:
  template <typename T>
  using Member = cppgc::Member<T>;
  template <typename T>
  using WeakMember = cppgc::WeakMember<T>;
  using PointeeType = T;

  constexpr ReplayWeakMember() = default;
  constexpr ReplayWeakMember(std::nullptr_t) {}           // NOLINT
  V8_INLINE ReplayWeakMember(T* raw) : weak_member_(raw) {
    if (ReplayLeakWeak) {
      strong_member_(raw);
    }
  }
  V8_INLINE ReplayWeakMember(T& raw)  // NOLINT
      : ReplayWeakMember(&raw) {}

#define REPLAY_WEAK_CTOR(ArgTypePre, ArgTypePost, Body)                        \
  V8_INLINE ReplayWeakMember(ArgTypePre ReplayWeakMember ArgTypePost other)    \
      : ReplayWeakMember(other.Get()) {                                        \
    Body                                                                       \
  }                                                                            \
  template <typename U,                                                        \
            std::enable_if_t<internal::IsDecayedSameV<T, U>>* = nullptr>       \
  V8_INLINE ReplayWeakMember(ArgTypePre ReplayWeakMember<U> ArgTypePost other) \
      : ReplayWeakMember(other.GetRawStorage()) {                              \
    Body                                                                       \
  }                                                                            \
  template <typename U,                                                        \
            std::enable_if_t<internal::IsStrictlyBaseOfV<T, U>>* = nullptr>    \
  V8_INLINE ReplayWeakMember(ArgTypePre ReplayWeakMember<U> ArgTypePost other) \
      : ReplayWeakMember(other.Get()) {                                        \
    Body                                                                       \
  }                                                                            \
  template <typename U, typename OtherBarrierPolicy,                           \
            typename OtherWeaknessTag, typename OtherCheckingPolicy,           \
            std::enable_if_t<internal::IsDecayedSameV<T, U>>* = nullptr>       \
  V8_INLINE ReplayWeakMember(                                                  \
      ArgTypePre BasicMember<U, OtherWeaknessTag, OtherBarrierPolicy,          \
                             OtherCheckingPolicy>                              \
          ArgTypePost other)                                                   \
      : ReplayWeakMember(other.GetRawStorage()) {                              \
    Body                                                                       \
  }                                                                            \
  template <typename U, typename OtherBarrierPolicy,                           \
            typename OtherWeaknessTag, typename OtherCheckingPolicy,           \
            std::enable_if_t<internal::IsStrictlyBaseOfV<T, U>>* = nullptr>    \
  V8_INLINE ReplayWeakMember(                                                  \
      ArgTypePre BasicMember<U, OtherWeaknessTag, OtherBarrierPolicy,          \
                        OtherCheckingPolicy>                                   \
          ArgTypePost other)                                                   \
      : ReplayWeakMember(other.Get()) {                                        \
    Body                                                                       \
  }

  // Copy ctors.
  REPLAY_WEAK_CTOR(const, &,)

  // Move ctors.
  REPLAY_WEAK_CTOR(, &&, other.Clear())

#undef REPLAY_WEAK_CTOR

  // Construction from Persistent.
  template <typename U, typename PersistentWeaknessPolicy,
            typename PersistentLocationPolicy,
            typename PersistentCheckingPolicy,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  V8_INLINE ReplayWeakMember(const BasicPersistent<U, PersistentWeaknessPolicy,
                                                   PersistentLocationPolicy,
                                                   PersistentCheckingPolicy>& p)
      : ReplayWeakMember(p.Get()) {}

  void Trace(cppgc::Visitor* visitor) const override {
    weak_member_.Trace(visitor);
    strong_member_.Trace(visitor);
  }

  // TODO: operator=
  // TODO: Swap + more

 private:
#if defined(CPPGC_POINTER_COMPRESSION)
  using RawStorage = CompressedPointer;
#else   // !defined(CPPGC_POINTER_COMPRESSION)
  using RawStorage = RawPointer;
#endif  // !defined(CPPGC_POINTER_COMPRESSION)

  V8_INLINE explicit ReplayWeakMember(RawStorage raw) {
    InitializingWriteBarrier(Get());
    this->CheckPointer(Get());
  }

  Member<T> strong_member_;
  WeakMember_<T> weak_member_;
};
}  // namespace internal

/**
 * ReplayWeakMembers are wrappers around WeakMember.
 * If 
 */
template <typename T>
using ReplayWeakMember = internal::ReplayWeakMember<T>;

}  // namespace cppgc

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MEMBER_REPLAY_H_
