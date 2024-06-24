// Copyright (c) 2024 Record Replay Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Loader wrappers for Replay Driver API functions.

#include <cstdarg>

#ifndef INCLUDE_REPLAYIO_INLINE_API_H
#define INCLUDE_REPLAYIO_INLINE_API_H

#if defined(V8_OS_STRING)
  #if defined(V8_OS_WIN)
  #define REPLAYIO_WIN 1
  #else
  #define REPLAYIO_WIN 0
  #endif
#else
  #if defined(BUILDFLAG)
    #if BUILDFLAG(IS_WIN)
    #define REPLAYIO_WIN 1
    #else
    #define REPLAYIO_WIN 0
    #endif
  #endif
#endif

#ifndef REPLAYIO_WIN
#error "Neither V8_OS_WIN nor BUILDFLAG was defined."
#endif

#if !REPLAYIO_WIN
#include <dlfcn.h>
#else
#include <windows.h>
#endif


#define REPLAY_API_DOES_NOT_EXIST (reinterpret_cast<void*>(1))

#define ForEachRecordReplaySymbol(Macro)                             \
  Macro(HasDisabledFeatures, (), (), bool, false)                    \
//   Macro(FeatureEnabled,                                           \
//         (const char* feature, const char* subfeature), bool, true)            \
//   Macro(HadMismatch, (), bool, false)                             \
//   Macro(Value, (const char* why, uintptr_t v), uintptr_t, v)      \
//   Macro(AreEventsDisallowed, (), bool, false)                     \
//   Macro(AreEventsPassedThrough, (), bool, false)                  \
//   Macro(AreAssertsDisabled, (), bool, false)                      \
//   Macro(CreateOrderedLock, (const char* name), size_t, 0)         \
//   Macro(IsReplaying, (), bool, false)                             \
//   Macro(HasDivergedFromRecording, (), bool, false)                \
//   Macro(NewDependencyGraphNode, (const char* json), int, 0)       \
//   Macro(AllowSideEffects, (), bool, true)                         \
//   Macro(PointerId, (const void* ptr), int, 0)                     \
//   Macro(IdPointer, (int id), void*, nullptr)                      \
//   Macro(GetRecordingId, (), char*, nullptr)                       \
//   Macro(GetUnusableRecordingReason, (), char*, nullptr)           \
//   Macro(NewBookmark, (), uint64_t, 0)                             \
//   Macro(PaintStart, (), size_t, 0)                                \
//   Macro(JSONCreateString, (const char*), void*, nullptr)          \
//   Macro(JSONCreateObject,                                         \
//         (size_t, const char**, void**), void*, nullptr)                       \
//   Macro(JSONToString, (void*), char*, nullptr)                    \
//   Macro(ProgressCounter, (), uint64_t*, nullptr)                  \
//   Macro(GetStack, (char* aStack, size_t aSize), bool, false)      \
//   Macro(ReadAssetFileContents,                                    \
//         (const char* aPath, size_t *aLength),                                 \
//         char*, nullptr)                                                       \
//   Macro(ReplaceSourceContents,                                    \
//         (const char* contents), const char*, nullptr)


#define ForEachRecordReplaySymbolVoid(Macro)                                         \
  Macro(Assert,                                         \
        (const char* format, va_list args),                             \
        (format, args))                                                 
  // Macro(AssertMaybeEventsDisallowed,                    \
  //       (const char* format, va_list args),                             \
  //       (format, args))                                                 \
  // Macro(AssertBytes,                                      \
  //       (const char* why, const void* buf, size_t size),                \
  //       (why, buf, size))                                               \
  // Macro(Print,                                          \
  //       (const char* format, va_list args),                             \
  //       (format, args))                                                 \
  // Macro(Warning,                                          \
  //       (const char* format, va_list args),                             \
  //       (format, args))                                                 \
  // Macro(Trace,                                            \
  //       (const char* format, va_list args),                             \
  //       (format, args))                                                 \
  // Macro(Crash,                                            \
  //       (const char* format, va_list args),                             \
  //       (format, args))                                                 \
  // Macro(Bytes,                                            \
  //       (const char* why, void* buf, size_t size),                      \
  //       (why, buf, size))                                               \
  // Macro(OrderedLock, (int lock), (lock))                  \
  // Macro(OrderedUnlock, (int lock), (lock))                \
  // Macro(NewCheckpoint, (), ())                            \
  // Macro(OnAnnotation,                                     \
  //       (const char* kind, const char* contents),                       \
  //       (kind, contents))                                               \
  // Macro(OnNetworkRequest,                                 \
  //   (const char* id, const char* kind, uint64_t bookmark),              \
  //   (id, kind, bookmark))                                               \
  // Macro(OnNetworkRequestEvent, (const char* id), (id))    \
  // Macro(OnNetworkStreamStart,                             \
  //       (const char* id, const char* kind, const char* parentId),       \
  //       (id, kind, parentId))                                           \
  // Macro(OnNetworkStreamData,                              \
  //       (const char* id, size_t offset, size_t length, uint64_t bookmark), \
  //       (id, offset, length, bookmark))                                 \
  // Macro(OnNetworkStreamEnd,                               \
  //       (const char* id, size_t length), (id, length))                  \
  // Macro(BeginDisallowEvents, (), ())                      \
  // Macro(BeginDisallowEventsWithLabel,                     \
  //       (const char* label), (label))                                   \
  // Macro(EndDisallowEvents, (), ())                        \
  // Macro(BeginPassThroughEvents, (), ())                   \
  // Macro(EndPassThroughEvents, (), ())                     \
  // Macro(RegisterPointer,                                  \
  //       (const char* name, const void* ptr), (name, ptr))               \
  // Macro(UnregisterPointer, (const void* ptr), (ptr))      \
  // Macro(BrowserEvent,                                     \
  //       (const char* name, const char* payload), (name, payload))       \
  // Macro(OnEvent,                                          \
  //       (const char* event, bool before), (event, before))              \
  // Macro(OnMouseEvent,                                     \
  //       (const char* kind, size_t clientX, size_t clientY, bool synthetic),\
  //       (kind, clientX, clientY, synthetic))                            \
  // Macro(OnKeyEvent,                                       \
  //       (const char* kind, const char* key, bool synthetic),            \
  //       (kind, key, synthetic))                                         \
  // Macro(OnNavigationEvent,                                \
  //       (const char* kind, const char* url), (kind, url))               \
  // Macro(AddDependencyGraphEdge,                           \
  //       (int source, int target, const char* json), (source, target, json)) \
  // Macro(BeginDependencyExecution, (int node), (node))     \
  // Macro(EndDependencyExecution, (), ())                   \
  // Macro(AddOrderedSRWLock,                                \
  //       (const char* name, void* lock), (name, lock))                   \
  // Macro(RemoveOrderedSRWLock, (void* lock), (lock))       \
  // Macro(MaybeTerminate,                                   \
  //       (void (*callback)(void*), void* data), (callback, data))        \
  // Macro(FinishRecording, (), ())                          \
  // Macro(GetCurrentJSStack,                                \
  //       (std::string* stackTrace), (stackTrace))                        \
  // Macro(EnterReplayCode, (), ())                          \
  // Macro(ExitReplayCode, (), ())                           \
  // Macro(BeginAssertBufferAllocations,                     \
  //   (const char* issueLabel), (issueLabel))                             \
  // Macro(EndAssertBufferAllocations, (), ())

namespace replayio {

template <typename Src, typename Dst>
static inline void CastPointer(const Src src, Dst* dst) {
  static_assert(sizeof(Src) == sizeof(uintptr_t), "bad size");
  static_assert(sizeof(Dst) == sizeof(uintptr_t), "bad size");
  memcpy((void*)dst, (const void*)&src, sizeof(uintptr_t));
}

static void* RecordReplayLoadSymbol(const char* name) {
#if !REPLAYIO_WIN
  void* fnptr = dlsym(RTLD_DEFAULT, name);
#else
  HMODULE module = GetModuleHandleA("windows-recordreplay.dll");
  void* fnptr = module ? (void*)GetProcAddress(module, name) : nullptr;
#endif
  return fnptr ? fnptr : REPLAY_API_DOES_NOT_EXIST;
}

#define DeclareRecordReplayApiFunction(Name, Params, Args, ReturnType, ReturnDefault)    \
  ReturnType Name Params { \
    static ReturnType (*fnptr) Params; \
    if (!fnptr) { \
      fnptr = reinterpret_cast<ReturnType(*)Params>(RecordReplayLoadSymbol("RecordReplay"#Name)); \
    } \
    if (fnptr != REPLAY_API_DOES_NOT_EXIST) { \
      return fnptr Args; \
    } \
    return ReturnDefault; \
  }
  ForEachRecordReplaySymbol(DeclareRecordReplayApiFunction)
#undef DeclareRecordReplayApiFunction

#define DeclareRecordReplayApiFunctionVoid(Name, Params, Args)    \
  void Name Params { \
    static void (*fnptr) Params; \
    if (!fnptr) { \
      fnptr = reinterpret_cast<void(*)Params>(RecordReplayLoadSymbol("RecordReplay"#Name)); \
    } \
    if (fnptr != REPLAY_API_DOES_NOT_EXIST) { \
      fnptr Args; \
    } \
  }
  ForEachRecordReplaySymbolVoid(DeclareRecordReplayApiFunctionVoid)
#undef DeclareRecordReplayApiFunctionVoid

} // namespace replayio

#endif  // INCLUDE_REPLAYIO_INLINE_API_H
