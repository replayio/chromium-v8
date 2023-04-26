// Copyright (c) 2023 Record Replay Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_MEMBER_REPLAY_H_
#define INCLUDE_CPPGC_MEMBER_REPLAY_H_

#include "cppgc/member.h"

namespace cppgc {

#define ReplayLeakWeak recordreplay::IsRecordingOrReplaying("avoid-weak-pointers")

/**
 * This is designed to have the same interface as |BaseMember| but
 * acts as a wrapper around a |WeakMember| and a |Member|.
 * Depending on |ReplayLeakWeak|, it will choose one over the other.
 * Choosing |Member| instead of |WeakMember| has the effect of eliminating
 * |WeakMember| which is depending on GC behavior, making it inherently
 * non-deterministic.
 *
 * https://linear.app/replay/issue/RUN-1457/deterministic-weakmember
 */
template<typename T>
class ReplayWeakMember : public GarbageCollectedMixin {
 public:
  using Member = cppgc::Member<T>;
  using WeakMember = cppgc::WeakMember<T>;
  using PointeeType = T;

  V8_INLINE constexpr ReplayWeakMember() = default;           // NOLINT
  V8_INLINE constexpr ReplayWeakMember(std::nullptr_t) {}     // NOLINT
  V8_INLINE ReplayWeakMember(internal::SentinelPointer s) {             // NOLINT
    if (ReplayLeakWeak) {
      strong_member_ = s;
    } else {
      weak_member_ = s;
    }
  }
  V8_INLINE ReplayWeakMember(T* raw) { // NOLINT
    if (ReplayLeakWeak) {
      strong_member_ = raw;
    } else {
      weak_member_ = raw;
    }
  }
  V8_INLINE ReplayWeakMember(T& raw) // NOLINT
      : ReplayWeakMember(&raw) {}

#define REPLAY_WEAK_CTOR(ArgTypePre, ArgTypePost, Body)                      \
  V8_INLINE ReplayWeakMember(ArgTypePre ReplayWeakMember ArgTypePost other) \
      : ReplayWeakMember(other.Get()) {                                      \
    Body                                                                     \
  }                                                                          \
  template <typename U,                                                      \
            std::enable_if_t<internal::IsDecayedSameV<T, U>>* = nullptr>     \
  V8_INLINE ReplayWeakMember(                                                \
      ArgTypePre ReplayWeakMember<U>ArgTypePost other)                       \
      : ReplayWeakMember(other.GetRawStorage()) {                            \
    Body                                                                     \
  }                                                                          \
  template <typename U,                                                      \
            std::enable_if_t<internal::IsStrictlyBaseOfV<T, U>>* = nullptr>  \
  V8_INLINE ReplayWeakMember(                                                \
      ArgTypePre ReplayWeakMember<U>ArgTypePost other)                       \
      : ReplayWeakMember(other.Get()) {                                      \
    Body                                                                     \
  }                                                                          \
  template <typename U, typename OtherBarrierPolicy,                         \
            typename OtherWeaknessTag, typename OtherCheckingPolicy,         \
            std::enable_if_t<internal::IsDecayedSameV<T, U>>* = nullptr>     \
  V8_INLINE ReplayWeakMember(                                                \
      ArgTypePre internal::BasicMember<U, OtherWeaknessTag, OtherBarrierPolicy,        \
                             OtherCheckingPolicy>ArgTypePost other)          \
      : ReplayWeakMember(other.GetRawStorage()) {                            \
    Body                                                                     \
  }                                                                          \
  template <typename U, typename OtherBarrierPolicy,                         \
            typename OtherWeaknessTag, typename OtherCheckingPolicy,         \
            std::enable_if_t<internal::IsStrictlyBaseOfV<T, U>>* = nullptr>  \
  V8_INLINE ReplayWeakMember(                                                \
      ArgTypePre internal::BasicMember<U, OtherWeaknessTag, OtherBarrierPolicy,        \
                             OtherCheckingPolicy>ArgTypePost other)          \
      : ReplayWeakMember(other.Get()) {                                      \
    Body                                                                     \
  }

  // Copy ctors.
  REPLAY_WEAK_CTOR(const, &,)

  // Move ctors.
  #define PARENTHESES () /* hackfix: https://stackoverflow.com/a/40054009 */
  REPLAY_WEAK_CTOR(, &&, other.Clear();)
  #undef PARENTHESES

#undef REPLAY_WEAK_CTOR

  // Construction from Persistent.
  template <typename U, typename PersistentWeaknessPolicy,
            typename PersistentLocationPolicy,
            typename PersistentCheckingPolicy,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  V8_INLINE ReplayWeakMember(const internal::BasicPersistent<U, PersistentWeaknessPolicy,
                                                   PersistentLocationPolicy,
                                                   PersistentCheckingPolicy>& p)
      : ReplayWeakMember(p.Get()) {}

  // Copy assignment.
  V8_INLINE ReplayWeakMember& operator=(const ReplayWeakMember& other) {
    return operator=(other.GetRawStorage());
  }

  // Heterogeneous copy assignment.
  template <typename U, typename OtherWeaknessTag, typename OtherBarrierPolicy,
            typename OtherCheckingPolicy>
  V8_INLINE ReplayWeakMember& operator=(
      const internal::BasicMember<U, OtherWeaknessTag, OtherBarrierPolicy,
                        OtherCheckingPolicy>& other) {
    if (ReplayLeakWeak) {
      return strong_member_.operator=(other);
    } else {
      return weak_member_.operator=(other);
    }
  }

  // Move assignment.
  V8_INLINE ReplayWeakMember& operator=(ReplayWeakMember&& other) noexcept {
    if (ReplayLeakWeak) {
      strong_member_.operator=(std::move(other.strong_member_));
    } else {
      weak_member_.operator=(std::move(other.weak_member_));
    }
    return *this;
  }

  // Heterogeneous move assignment.
  template <typename U, typename OtherWeaknessTag, typename OtherBarrierPolicy,
            typename OtherCheckingPolicy>
  V8_INLINE ReplayWeakMember& operator=(
      internal::BasicMember<U, OtherWeaknessTag, OtherBarrierPolicy,
                  OtherCheckingPolicy>&& other) noexcept {
    if (ReplayLeakWeak) {
      return strong_member_.operator=(std::move(other));
    } else {
      return weak_member_.operator=(std::move(other));
    }
  }

  // Assignment from Persistent.
  template <typename U, typename PersistentWeaknessPolicy,
            typename PersistentLocationPolicy,
            typename PersistentCheckingPolicy,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  V8_INLINE ReplayWeakMember& operator=(
      const internal::BasicPersistent<U, PersistentWeaknessPolicy,
                            PersistentLocationPolicy, PersistentCheckingPolicy>&
          other) {
    if (ReplayLeakWeak) {
      strong_member_.operator=(other);
    } else {
      weak_member_.operator=(other);
    }
    return *this;
  }

  V8_INLINE ReplayWeakMember& operator=(T* other) {
    if (ReplayLeakWeak) {
      strong_member_.operator=(other);
    } else {
      weak_member_.operator=(other);
    }
    return *this;
  }
  V8_INLINE ReplayWeakMember& operator=(std::nullptr_t) {
    if (ReplayLeakWeak) {
      strong_member_.operator=(nullptr);
    } else {
      weak_member_.operator=(nullptr);
    }
    return *this;
  }
  V8_INLINE ReplayWeakMember& operator=(internal::SentinelPointer s) {
    if (ReplayLeakWeak) {
      strong_member_.operator=(s);
    } else {
      weak_member_.operator=(s);
    }
    return *this;
  }

  V8_INLINE void Swap(ReplayWeakMember& other) {
    if (ReplayLeakWeak) {
      strong_member_.Swap(other.strong_member_);
    } else {
      weak_member_.Swap(other.weak_member_);
    }
  }

  template <typename OtherWeaknessTag, typename OtherBarrierPolicy,
            typename OtherCheckingPolicy>
  V8_INLINE void Swap(internal::BasicMember<T, OtherWeaknessTag, OtherBarrierPolicy,
                                  OtherCheckingPolicy>& other) {
    if (ReplayLeakWeak) {
      strong_member_.Swap(other);
    } else {
      weak_member_.Swap(other);
    }
  }

  V8_INLINE explicit operator bool() const {
    if (ReplayLeakWeak) {
      return !!strong_member_;
    } else {
      return !!weak_member_;
    }
  }
  V8_INLINE operator T*() const {
    if (ReplayLeakWeak) {
      return (T*)strong_member_;
    } else {
      return (T*)weak_member_;
    }
  }
  V8_INLINE T* operator->() const {
    if (ReplayLeakWeak) {
      return strong_member_.operator->();
    } else {
      return weak_member_.operator->();
    }
  }
  V8_INLINE T& operator*() const {
    if (ReplayLeakWeak) {
      return strong_member_.operator*();
    } else {
      return weak_member_.operator*();
    }
  }

  // CFI cast exemption to allow passing SentinelPointer through T* and support
  // heterogeneous assignments between different Member and Persistent handles
  // based on their actual types.
  V8_INLINE V8_CLANG_NO_SANITIZE("cfi-unrelated-cast") T* Get() const {
    if (ReplayLeakWeak) {
      return strong_member_.Get();
    } else {
      return weak_member_.Get();
    }
  }

  V8_INLINE void Clear() {
    if (ReplayLeakWeak) {
      strong_member_.Clear();
    } else {
      weak_member_.Clear();
    }
  }

  V8_INLINE T* Release() {
    if (ReplayLeakWeak) {
      return strong_member_.Release();
    } else {
      return weak_member_.Release();
    }
  }

  V8_INLINE internal::MemberBase::RawStorage GetRawStorage() const {
    if (ReplayLeakWeak) {
      return strong_member_.GetRawStorage();
    } else {
      return weak_member_.GetRawStorage();
    }
  }

  void Trace(cppgc::Visitor* visitor) const override {
    weak_member_.Trace(visitor);
    strong_member_.Trace(visitor);
  }

 private:
  V8_INLINE explicit ReplayWeakMember(internal::MemberBase::RawStorage raw) {
    if (ReplayLeakWeak) {
      strong_member_ = raw;
    } else {
      weak_member_ = raw;
    }
  }

  Member strong_member_;
  WeakMember weak_member_;
};

// TODO: comparison operators

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_MEMBER_REPLAY_H_
