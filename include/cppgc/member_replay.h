// Copyright (c) 2023 Record Replay Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_MEMBER_REPLAY_H_
#define INCLUDE_CPPGC_MEMBER_REPLAY_H_

namespace cppgc {
namespace internal {
#define ReplayLeakWeak recordreplay::IsRecordingOrReplaying("avoid-weak-pointers")

/**
 * This is a copy of the |BaseMember| interface.
 * It acts as a wrapper around a |WeakMember| plus an optional strong 
 * |Member| reference.
 * https://linear.app/replay/issue/RUN-1457/deterministic-weakmember
 */
template<typename T>
class ReplayWeakMember : public GarbageCollectedMixin {
 public:
  using Member = cppgc::Member<T>;
  using WeakMember = cppgc::WeakMember<T>;
  using PointeeType = T;

  constexpr ReplayWeakMember() = default;
  constexpr ReplayWeakMember(std::nullptr_t) {}           // NOLINT
  V8_INLINE ReplayWeakMember(T* raw) : weak_member_(raw) {
    if (ReplayLeakWeak) {
      strong_member_ = raw;
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

  // Copy assignment.
  V8_INLINE ReplayWeakMember& operator=(const ReplayWeakMember& other) {
    return operator=(other.GetRawStorage());
  }

  // TODO: override all |operator=| to also assign to/from |ReplayWeakMember|

  // Heterogeneous copy assignment. When the source pointer have a different
  // type, perform a compress-decompress round, because the source pointer may
  // need to be adjusted.
  template <typename U, typename OtherWeaknessTag, typename OtherBarrierPolicy,
            typename OtherCheckingPolicy>
  V8_INLINE WeakMember& operator=(
      const BasicMember<U, OtherWeaknessTag, OtherBarrierPolicy,
                        OtherCheckingPolicy>& other) {
    if (ReplayLeakWeak) {
      strong_member_.operator=(other);
    }
    return weak_member_.operator=(other);
  }

  // Move assignment.
  V8_INLINE ReplayWeakMember& operator=(ReplayWeakMember&& other) noexcept {
    if (ReplayLeakWeak) {
      strong_member_.operator=(other.strong_member_);
    }
    weak_member_.operator=(other.weak_member_);
    other.Clear();
    return *this;
  }

  // Heterogeneous move assignment. When the source pointer have a different
  // type, perform a compress-decompress round, because the source pointer may
  // need to be adjusted.
  template <typename U, typename OtherWeaknessTag, typename OtherBarrierPolicy,
            typename OtherCheckingPolicy>
  V8_INLINE WeakMember& operator=(
      BasicMember<U, OtherWeaknessTag, OtherBarrierPolicy,
                  OtherCheckingPolicy>&& other) noexcept {
    if (ReplayLeakWeak) {
      strong_member_.operator=(other);
    }
    return weak_member_.operator=(other);
  }

  // Assignment from Persistent.
  template <typename U, typename PersistentWeaknessPolicy,
            typename PersistentLocationPolicy,
            typename PersistentCheckingPolicy,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  V8_INLINE WeakMember& operator=(
      const BasicPersistent<U, PersistentWeaknessPolicy,
                            PersistentLocationPolicy, PersistentCheckingPolicy>&
          other) {
    if (ReplayLeakWeak) {
      strong_member_.operator=(other);
    }
    return weak_member_.operator=(other);
  }

  V8_INLINE WeakMember& operator=(T* other) {
    if (ReplayLeakWeak) {
      strong_member_.operator=(other);
    }
    return weak_member_.operator=(other);
  }

  V8_INLINE WeakMember& operator=(std::nullptr_t) {
    if (ReplayLeakWeak) {
      strong_member_.operator=(std::nullptr);
    }
    return weak_member_.operator=(std::nullptr);
  }
  V8_INLINE WeakMember& operator=(SentinelPointer s) {
    if (ReplayLeakWeak) {
      strong_member_.operator=(s);
    }
    return weak_member_.operator=(s);
  }

  V8_INLINE void Swap(ReplayWeakMember& other) {
    // NOTE: For some reason, this does not need to be atomic -
    // but |operator=| overloads do? 🤷‍♀️
    weak_member_.Swap(other.weak_member_);
    if (ReplayLeakWeak) {
      strong_member_.Swap(other.strong_member_);
    }
  }

  template <typename OtherWeaknessTag, typename OtherBarrierPolicy,
            typename OtherCheckingPolicy>
  V8_INLINE void Swap(BasicMember<T, OtherWeaknessTag, OtherBarrierPolicy,
                                  OtherCheckingPolicy>& other) {
    weak_member_.Swap(other);
    if (ReplayLeakWeak) {
      strong_member_ = weak_member_;
    }
  }

  V8_INLINE explicit operator bool() const { return !!weak_member_; }
  V8_INLINE operator T*() const { return weak_member_.Get(); }
  V8_INLINE T* operator->() const { return weak_member_.operator->(); }
  V8_INLINE T& operator*() const { return weak_member_.operator*(); }

  // CFI cast exemption to allow passing SentinelPointer through T* and support
  // heterogeneous assignments between different Member and Persistent handles
  // based on their actual types.
  V8_INLINE V8_CLANG_NO_SANITIZE("cfi-unrelated-cast") T* Get() const { 
    return weak_member_.Get();
  }

  V8_INLINE void Clear() {
    // TODO: Clear must be atomic or ensured to be main-thread only
    if (ReplayLeakWeak) {
      // Same logic as |Release|
      strong_member_.Clear();
    }
    weak_member_.Clear();
  }

  V8_INLINE T* Release() {
    if (ReplayLeakWeak) {
      // 1. Not sure why one would ever want to call |Release| on a |WeakMember|,
      // since it does not hold ownership of anything.
      // 2. Also, in theory, we don't want to release the |strong_member_| here,
      // if a |WeakMember| is released deterministically. Since we don't want it
      // keep leaking. However, often times |Release| is used (on |Member|) in 
      // conjunction with an eager resource clean-up (e.g. in `dom_timer.cc`),
      // or right before transferring ownership. So we don't want to keep it.
      strong_member_.Release();
    }
    return weak_member_.Release();
  }

  V8_INLINE RawStorage GetRawStorage() const {
    return weak_member_.GetRawStorage();
  }

  void Trace(cppgc::Visitor* visitor) const override {
    weak_member_.Trace(visitor);
    strong_member_.Trace(visitor);
  }

 private:
#if defined(CPPGC_POINTER_COMPRESSION)
  using RawStorage = CompressedPointer;
#else   // !defined(CPPGC_POINTER_COMPRESSION)
  using RawStorage = RawPointer;
#endif  // !defined(CPPGC_POINTER_COMPRESSION)

  V8_INLINE explicit ReplayWeakMember(RawStorage raw) : weak_member_(raw) {
    if (ReplayLeakWeak) {
      strong_member_ = raw;
    }
  }

  Member<T> strong_member_;
  WeakMember<T> weak_member_;
};
}  // namespace internal

template <typename T>
using ReplayWeakMember = internal::ReplayWeakMember<T>;

// TODO: comparison operators

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_MEMBER_REPLAY_H_
