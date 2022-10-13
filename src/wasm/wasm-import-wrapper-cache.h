// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_IMPORT_WRAPPER_CACHE_H_
#define V8_WASM_WASM_IMPORT_WRAPPER_CACHE_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include <unordered_map>

#include "include/v8.h"  // For replay.
#include "src/base/platform/mutex.h"
#include "src/wasm/module-instantiate.h"
#include "src/wasm/wasm-code-manager.h"

namespace v8::internal::wasm {

class WasmCode;
class WasmEngine;
class WasmImportWrapperHandle;

// Implements a cache for import wrappers.
class WasmImportWrapperCache {
 public:
  struct CacheKey {
    CacheKey(ImportCallKind kind, CanonicalTypeIndex type_index,
             int expected_arity, Suspend suspend)
        : kind(kind),
          type_index(type_index),
          expected_arity(expected_arity),
          suspend(suspend) {}

    bool operator==(const CacheKey& rhs) const = default;

    ImportCallKind kind;
    CanonicalTypeIndex type_index;
    int expected_arity;
    Suspend suspend;
  };

  class CacheKeyHash {
   public:
    size_t operator()(const CacheKey& key) const {
#if V8_HASHES_COLLIDE
      if (v8_flags.hashes_collide) return base::kCollidingHash;
#endif  // V8_HASHES_COLLIDE

      return base::hash_combine(static_cast<uint8_t>(key.kind),
                                key.type_index.index, key.expected_arity);
    }
  };

  // Helper class to modify the cache under a lock.
  class V8_NODISCARD ModificationScope {
   public:
    explicit ModificationScope(WasmImportWrapperCache* cache)
        : cache_(cache),
          ordered_(record_replay_ordered_lock_),
          guard_(&cache->mutex_) {}

    WasmCode* AddWrapper(
        WasmCompilationResult result, WasmCode::Kind kind,
        uint64_t signature_hash,
        std::shared_ptr<wasm::WasmImportWrapperHandle> wrapper_handle);

   private:
    WasmImportWrapperCache* const cache_;
    // Declared before guard_ so the recorded order is taken first; see the
    // constructor in wasm-import-wrapper-cache.cc.
    v8::recordreplay::AutoOrderedLock ordered_;
    base::MutexGuard guard_;
  };

  WasmImportWrapperCache();
  ~WasmImportWrapperCache() = default;

  void LazyInitialize(Isolate* triggering_isolate);

  void Free(std::vector<WasmCode*>& wrappers);

  V8_EXPORT_PRIVATE std::shared_ptr<WasmImportWrapperHandle> Get(
      Isolate* isolate, ImportCallKind kind, CanonicalTypeIndex type_index,
      int expected_arity, Suspend suspend, const wasm::CanonicalSig* sig);

  V8_EXPORT_PRIVATE
  std::shared_ptr<WasmImportWrapperHandle> GetCompiled(
      Isolate* isolate, ImportCallKind kind, CanonicalTypeIndex type_index,
      int expected_arity, Suspend suspend, const wasm::CanonicalSig* sig);

  V8_EXPORT_PRIVATE
  std::shared_ptr<WasmImportWrapperHandle> CompileWasmJsFastCallWrapper(
      Isolate* isolate, DirectHandle<JSReceiver> callable,
      const wasm::CanonicalSig* sig);

  WasmCode* Lookup(Address pc) const;

  void LogForIsolate(Isolate* isolate);

  size_t EstimateCurrentMemoryConsumption() const;

  V8_EXPORT_PRIVATE bool HasCodeForTesting(ImportCallKind kind,
                                           CanonicalTypeIndex type_index,
                                           int expected_arity, Suspend suspend);

#ifdef DEBUG
  V8_EXPORT_PRIVATE bool IsCompiledWrapper(WasmCodePointer code_pointer);
#endif

 private:
  std::optional<Builtin> BuiltinForWrapper(ImportCallKind kind,
                                           const wasm::CanonicalSig* sig,
                                           Suspend suspend);

  std::shared_ptr<WasmImportWrapperHandle> CompileWrapper(
      Isolate* isolate, const CacheKey& cache_key,
      const wasm::CanonicalSig* sig,
      std::shared_ptr<WasmImportWrapperHandle> optional_handle);

  std::unique_ptr<WasmCodeAllocator> code_allocator_;
  mutable base::Mutex mutex_;
  // Orders the acquisitions that compile or publish a wrapper, rather than the
  // mutex itself; see the constructor in wasm-import-wrapper-cache.cc. One lock
  // for the whole process: V8 13.0 made this cache a process-wide singleton.
  // Not taken by Lookup(), Get(), GetCompiled() or HasCodeForTesting(), which
  // read under mutex_ and which every stack walk reaches. Static, so the class
  // keeps upstream's size.
  static int record_replay_ordered_lock_;
  std::unordered_map<CacheKey, std::weak_ptr<WasmImportWrapperHandle>,
                     CacheKeyHash>
      entry_map_;

  // Lookup support. The map key is the instruction start address.
  std::map<Address, WasmCode*> codes_;

  friend class WasmImportWrapperHandle;
};

class WasmImportWrapperHandle {
 public:
  WasmImportWrapperHandle(Address addr, uint64_t signature_hash);
  WasmImportWrapperHandle(WasmCode* code, Address addr,
                          uint64_t signature_hash);
  WasmImportWrapperHandle(const WasmImportWrapperHandle&) = delete;
  WasmImportWrapperHandle& operator=(const WasmImportWrapperHandle&) = delete;

  ~WasmImportWrapperHandle();

  WasmCodePointer code_pointer() const { return code_pointer_; }
  const WasmCode* code() const { return code_.load(std::memory_order_acquire); }
  bool has_code() const {
    return code_.load(std::memory_order_acquire) != nullptr;
  }

 private:
  void set_code(WasmCode* code);

  const WasmCodePointer code_pointer_;
  std::atomic<WasmCode*> code_ = nullptr;

  friend class WasmImportWrapperCache::ModificationScope;
};

}  // namespace v8::internal::wasm

#endif  // V8_WASM_WASM_IMPORT_WRAPPER_CACHE_H_
