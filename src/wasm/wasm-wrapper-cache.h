// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_WRAPPER_CACHE_H_
#define V8_WASM_WASM_WRAPPER_CACHE_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include <unordered_map>

#include "include/v8.h"  // For replay.
#include "src/base/platform/mutex.h"
#include "src/logging/counters.h"
#include "src/wasm/module-instantiate.h"
#include "src/wasm/wasm-code-manager.h"

namespace v8::internal::wasm {

class WasmCode;
class WasmWrapperHandle;

// Implements a cache for import wrappers.
template <typename CacheKey>
class WasmWrapperCache {
 public:
  // Helper class to modify the cache under a lock.
  class V8_NODISCARD ModificationScope {
   public:
    explicit ModificationScope(WasmWrapperCache<CacheKey>* cache)
        : cache_(cache),
          ordered_(record_replay_ordered_lock_),
          guard_(&cache->mutex_) {}

    WasmCode* AddWrapper(
        WasmCompilationResult result, WasmCode::Kind kind,
        uint64_t signature_hash,
        std::shared_ptr<wasm::WasmWrapperHandle> wrapper_handle);

   private:
    WasmWrapperCache* const cache_;
    // Declared before guard_ so the recorded order is taken first; see the
    // constructor below.
    v8::recordreplay::AutoOrderedLock ordered_;
    base::MutexGuard guard_;
  };

  WasmWrapperCache() : code_allocator_{&counter_updates_} {
    // Order the acquisitions that compile or publish a wrapper, rather than the
    // mutex itself. V8 13.0 made this cache process wide and put it on the stack
    // walk: WasmCodeManager::LookupCode(pc) falls through to Lookup() for every
    // pc that is in no NativeModule, so an ordered mutex would be taken by every
    // stack walk in every program, wasm or not -- including the walks Replay's
    // own CDP handling does with events disallowed, where there is no order to
    // record. Recording and replaying would then disagree about whether the lock
    // was taken at all.
    //
    // Only {ModificationScope} takes it, which is what the fork's ordering was
    // for: that scope holds mutex_ while a wrapper is published, and
    // AddWrapper() -> WasmWrapperHandle::set_code() is the only path that gives a
    // handle its code.
    //
    // The read paths -- Lookup(), Get(), GetCompiled(), HasCodeForTesting() --
    // take mutex_ alone and do not take this lock, deliberately (see above).
    // V8 13.8 reshaped this cache and two things about it are recorded in defect
    // DV8-003 rather than here: a wrapper's code is now read through
    // WasmImportWrapperHandle::code(), a relaxed atomic load with no lock at all,
    // and Get()'s builtin branch publishes an entry_map_ entry under mutex_
    // WITHOUT this ordered lock -- that entry never carries compiled code, so the
    // sentence above still holds for everything this lock was protecting.
    //
    // This runs once per process per instantiation, from
    // WasmEngine::InitializeOncePerProcess by way of GlobalWasmState, which
    // holds both caches as members (wasm-engine.cc) -- the same kind of point as
    // NativeModuleCache's own ordered lock in wasm-engine.cc.
    //
    // V8 14.5 split this cache into a template base and a derived
    // WasmImportWrapperCache, and the lock moved here, onto the base, because the
    // publish path moved here with it: CacheCompiledWrapper() in
    // wasm-wrapper-cache-inl.h constructs the ModificationScope for EVERY
    // instantiation, so a lock left on the derived class would no longer cover
    // the import cache's own publication. A static member of a class template is
    // one object per instantiation, so each cache gets its own lock id; see
    // defect DV8-032 for the consequences, which include the stack-entry wrapper
    // cache acquiring an ordering it did not have before M145.
    record_replay_ordered_lock_ =
        static_cast<int>(recordreplay::CreateOrderedLock("WasmImportWrapperCache"));
  }
  ~WasmWrapperCache() = default;

  void Free(std::vector<WasmCode*>& wrappers);

  V8_EXPORT_PRIVATE std::shared_ptr<WasmWrapperHandle> GetCompiled(
      Isolate* isolate, CacheKey cache_key);

  WasmCode* Lookup(Address pc) const;

  void LogForIsolate(Isolate* isolate);

  size_t EstimateCurrentMemoryConsumption() const;

  // Call this from time to time (after creating / tiering up import wrappers)
  // to publish delayed counter updates in the given isolate.
  void PublishCounterUpdates(Isolate* isolate) {
    counter_updates_.Publish(isolate);
  }

 protected:
  virtual std::pair<WasmCompilationResult, WasmCode::Kind> CompileWrapper(
      Isolate* isolate, const CacheKey& cache_key) = 0;

  std::shared_ptr<WasmWrapperHandle> CacheCompiledWrapper(
      Isolate* isolate, WasmCompilationResult, WasmCode::Kind, CacheKey,
      std::shared_ptr<WasmWrapperHandle>);

  WasmCodeAllocator code_allocator_;
  mutable base::Mutex mutex_;
  // Orders the acquisitions that compile or publish a wrapper, rather than the
  // mutex itself; see the constructor above. One lock per cache: V8 13.0 made
  // this cache a process-wide singleton and V8 14.5 made it a template with one
  // instantiation per wrapper kind.
  // Not taken by Lookup(), Get(), GetCompiled() or HasCodeForTesting(), which
  // read under mutex_ and which every stack walk reaches. Static, so the class
  // keeps upstream's size -- which the unconditional
  // static_assert(sizeof(WasmWrapperCache<ImportWrapperCacheKey>) ==
  // sizeof(WasmImportWrapperCache)) in wasm-import-wrapper-cache.h enforces.
  static int record_replay_ordered_lock_;
  std::unordered_map<CacheKey, std::weak_ptr<WasmWrapperHandle>,
                     typename CacheKey::Hash>
      entry_map_;

  // Lookup support. The map key is the instruction start address.
  std::map<Address, WasmCode*> codes_;

  // Since the cache is not owned by a single isolate, we store histogram
  // samples and publish them in the next best isolate.
  DelayedCounterUpdates counter_updates_;

  friend class WasmWrapperHandle;
};

template <typename CacheKey>
int WasmWrapperCache<CacheKey>::record_replay_ordered_lock_ = 0;

class WasmWrapperHandle {
 public:
  WasmWrapperHandle(Address addr, uint64_t signature_hash);
  WasmWrapperHandle(WasmCode* code, Address addr, uint64_t signature_hash);
  WasmWrapperHandle(const WasmWrapperHandle&) = delete;
  WasmWrapperHandle& operator=(const WasmWrapperHandle&) = delete;

  ~WasmWrapperHandle();

  WasmCodePointer code_pointer() const { return code_pointer_; }
  const WasmCode* code() const { return code_.load(std::memory_order_acquire); }
  bool has_code() const {
    return code_.load(std::memory_order_acquire) != nullptr;
  }

  void set_code(WasmCode* code);

 private:
  const WasmCodePointer code_pointer_;
  std::atomic<WasmCode*> code_ = nullptr;
};

}  //  namespace v8::internal::wasm

#endif  // V8_WASM_WASM_WRAPPER_CACHE_H_
