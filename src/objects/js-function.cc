// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/js-function.h"

#include "src/baseline/baseline-batch-compiler.h"
#include "src/codegen/compiler.h"
#include "src/diagnostics/code-tracer.h"
#include "src/execution/isolate.h"
#include "src/execution/tiering-manager.h"
#include "src/heap/heap-inl.h"
#include "src/ic/ic.h"
#include "src/init/bootstrapper.h"
#include "src/objects/feedback-cell-inl.h"
#include "src/strings/string-builder-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

#include "replayio.h"
#include "src/replay/replayio.h"

namespace v8 {
namespace internal {

CodeKinds JSFunction::GetAttachedCodeKinds() const {
  const CodeKind kind = code().kind();
  if (!CodeKindIsJSFunction(kind)) return {};
  if (CodeKindIsOptimizedJSFunction(kind) &&
      code().marked_for_deoptimization()) {
    return {};
  }
  return CodeKindToCodeKindFlag(kind);
}

CodeKinds JSFunction::GetAvailableCodeKinds() const {
  CodeKinds result = GetAttachedCodeKinds();

  if ((result & CodeKindFlag::INTERPRETED_FUNCTION) == 0) {
    // The SharedFunctionInfo could have attached bytecode.
    if (shared().HasBytecodeArray()) {
      result |= CodeKindFlag::INTERPRETED_FUNCTION;
    }
  }

  if ((result & CodeKindFlag::BASELINE) == 0) {
    // The SharedFunctionInfo could have attached baseline code.
    if (shared().HasBaselineCode()) {
      result |= CodeKindFlag::BASELINE;
    }
  }

  // Check the optimized code cache.
  if (has_feedback_vector() && feedback_vector().has_optimized_code() &&
      !feedback_vector().optimized_code().marked_for_deoptimization()) {
    CodeT code = feedback_vector().optimized_code();
    DCHECK(CodeKindIsOptimizedJSFunction(code.kind()));
    result |= CodeKindToCodeKindFlag(code.kind());
  }

  DCHECK_EQ((result & ~kJSFunctionCodeKindsMask), 0);
  return result;
}

bool JSFunction::HasAttachedOptimizedCode() const {
  CodeKinds result = GetAttachedCodeKinds();
  return (result & kOptimizedJSFunctionCodeKindsMask) != 0;
}

bool JSFunction::HasAvailableHigherTierCodeThan(CodeKind kind) const {
  return HasAvailableHigherTierCodeThanWithFilter(kind,
                                                  kJSFunctionCodeKindsMask);
}

bool JSFunction::HasAvailableHigherTierCodeThanWithFilter(
    CodeKind kind, CodeKinds filter_mask) const {
  const int kind_as_int_flag = static_cast<int>(CodeKindToCodeKindFlag(kind));
  DCHECK(base::bits::IsPowerOfTwo(kind_as_int_flag));
  // Smear right - any higher present bit means we have a higher tier available.
  const int mask = kind_as_int_flag | (kind_as_int_flag - 1);
  const CodeKinds masked_available_kinds =
      GetAvailableCodeKinds() & filter_mask;
  return (masked_available_kinds & static_cast<CodeKinds>(~mask)) != 0;
}

bool JSFunction::HasAvailableOptimizedCode() const {
  CodeKinds result = GetAvailableCodeKinds();
  return (result & kOptimizedJSFunctionCodeKindsMask) != 0;
}

bool JSFunction::HasAttachedCodeKind(CodeKind kind) const {
  CodeKinds result = GetAttachedCodeKinds();
  return (result & CodeKindToCodeKindFlag(kind)) != 0;
}

bool JSFunction::HasAvailableCodeKind(CodeKind kind) const {
  CodeKinds result = GetAvailableCodeKinds();
  return (result & CodeKindToCodeKindFlag(kind)) != 0;
}

namespace {

// Returns false if no highest tier exists (i.e. the function is not compiled),
// otherwise returns true and sets highest_tier.
V8_WARN_UNUSED_RESULT bool HighestTierOf(CodeKinds kinds,
                                         CodeKind* highest_tier) {
  DCHECK_EQ((kinds & ~kJSFunctionCodeKindsMask), 0);
  // Higher tiers > lower tiers.
  static_assert(CodeKind::TURBOFAN > CodeKind::INTERPRETED_FUNCTION);
  if (kinds == 0) return false;
  const int highest_tier_log2 =
      31 - base::bits::CountLeadingZeros(static_cast<uint32_t>(kinds));
  DCHECK(CodeKindIsJSFunction(static_cast<CodeKind>(highest_tier_log2)));
  *highest_tier = static_cast<CodeKind>(highest_tier_log2);
  return true;
}

}  // namespace

base::Optional<CodeKind> JSFunction::GetActiveTier() const {
#if V8_ENABLE_WEBASSEMBLY
  // Asm/Wasm functions are currently not supported. For simplicity, this
  // includes invalid asm.js functions whose code hasn't yet been updated to
  // CompileLazy but is still the InstantiateAsmJs builtin.
  if (shared().HasAsmWasmData() ||
      code().builtin_id() == Builtin::kInstantiateAsmJs) {
    return {};
  }
#endif  // V8_ENABLE_WEBASSEMBLY

  CodeKind highest_tier;
  if (!HighestTierOf(GetAvailableCodeKinds(), &highest_tier)) return {};

#ifdef DEBUG
  CHECK(highest_tier == CodeKind::TURBOFAN ||
        highest_tier == CodeKind::BASELINE ||
        highest_tier == CodeKind::MAGLEV ||
        highest_tier == CodeKind::INTERPRETED_FUNCTION);

  if (highest_tier == CodeKind::INTERPRETED_FUNCTION) {
    CHECK(code().is_interpreter_trampoline_builtin() ||
          (CodeKindIsOptimizedJSFunction(code().kind()) &&
           code().marked_for_deoptimization()) ||
          (code().builtin_id() == Builtin::kCompileLazy &&
           shared().HasBytecodeArray() && !shared().HasBaselineCode()));
  }
#endif  // DEBUG

  return highest_tier;
}

bool JSFunction::ActiveTierIsIgnition() const {
  return GetActiveTier() == CodeKind::INTERPRETED_FUNCTION;
}

bool JSFunction::ActiveTierIsBaseline() const {
  return GetActiveTier() == CodeKind::BASELINE;
}

bool JSFunction::ActiveTierIsMaglev() const {
  return GetActiveTier() == CodeKind::MAGLEV;
}

bool JSFunction::ActiveTierIsTurbofan() const {
  return GetActiveTier() == CodeKind::TURBOFAN;
}

bool JSFunction::CanDiscardCompiled() const {
  // Essentially, what we are asking here is, has this function been compiled
  // from JS code? We can currently tell only indirectly, by looking at
  // available code kinds. If any JS code kind exists, we can discard.
  //
  // Attached optimized code that is marked for deoptimization will not show up
  // in the list of available code kinds, thus we must check for it manually.
  //
  // Note that when the function has not yet been compiled we also return
  // false; that's fine, since nothing must be discarded in that case.
  if (CodeKindIsOptimizedJSFunction(code().kind())) return true;
  CodeKinds result = GetAvailableCodeKinds();
  return (result & kJSFunctionCodeKindsMask) != 0;
}

namespace {

constexpr TieringState TieringStateFor(CodeKind target_kind,
                                       ConcurrencyMode mode) {
  DCHECK(target_kind == CodeKind::MAGLEV || target_kind == CodeKind::TURBOFAN);
  return target_kind == CodeKind::MAGLEV
             ? (IsConcurrent(mode) ? TieringState::kRequestMaglev_Concurrent
                                   : TieringState::kRequestMaglev_Synchronous)
             : (IsConcurrent(mode)
                    ? TieringState::kRequestTurbofan_Concurrent
                    : TieringState::kRequestTurbofan_Synchronous);
}

}  // namespace

void JSFunction::MarkForOptimization(Isolate* isolate, CodeKind target_kind,
                                     ConcurrencyMode mode) {
  if (!isolate->concurrent_recompilation_enabled() ||
      isolate->bootstrapper()->IsActive()) {
    mode = ConcurrencyMode::kSynchronous;
  }

  DCHECK(CodeKindIsOptimizedJSFunction(target_kind));
  DCHECK(!is_compiled() || ActiveTierIsIgnition() || ActiveTierIsBaseline() ||
         ActiveTierIsMaglev());
  DCHECK(!ActiveTierIsTurbofan());
  DCHECK(shared().HasBytecodeArray());
  DCHECK(shared().allows_lazy_compilation() ||
         !shared().optimization_disabled());

  if (IsConcurrent(mode)) {
    if (IsInProgress(tiering_state())) {
      if (v8_flags.trace_concurrent_recompilation) {
        PrintF("  ** Not marking ");
        ShortPrint();
        PrintF(" -- already in optimization queue.\n");
      }
      return;
    }
    if (v8_flags.trace_concurrent_recompilation) {
      PrintF("  ** Marking ");
      ShortPrint();
      PrintF(" for concurrent %s recompilation.\n",
             CodeKindToString(target_kind));
    }
  }

  set_tiering_state(TieringStateFor(target_kind, mode));
}

void JSFunction::SetInterruptBudget(Isolate* isolate) {
  raw_feedback_cell().set_interrupt_budget(
      TieringManager::InterruptBudgetFor(isolate, *this));
}

// static
Maybe<bool> JSFunctionOrBoundFunctionOrWrappedFunction::CopyNameAndLength(
    Isolate* isolate,
    Handle<JSFunctionOrBoundFunctionOrWrappedFunction> function,
    Handle<JSReceiver> target, Handle<String> prefix, int arg_count) {
  // Setup the "length" property based on the "length" of the {target}.
  // If the targets length is the default JSFunction accessor, we can keep the
  // accessor that's installed by default on the
  // JSBoundFunction/JSWrappedFunction. It lazily computes the value from the
  // underlying internal length.
  Handle<AccessorInfo> function_length_accessor =
      isolate->factory()->function_length_accessor();
  LookupIterator length_lookup(isolate, target,
                               isolate->factory()->length_string(), target,
                               LookupIterator::OWN);
  if (!target->IsJSFunction() ||
      length_lookup.state() != LookupIterator::ACCESSOR ||
      !length_lookup.GetAccessors().is_identical_to(function_length_accessor)) {
    Handle<Object> length(Smi::zero(), isolate);
    Maybe<PropertyAttributes> attributes =
        JSReceiver::GetPropertyAttributes(&length_lookup);
    if (attributes.IsNothing()) return Nothing<bool>();
    if (attributes.FromJust() != ABSENT) {
      Handle<Object> target_length;
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, target_length,
                                       Object::GetProperty(&length_lookup),
                                       Nothing<bool>());
      if (target_length->IsNumber()) {
        length = isolate->factory()->NewNumber(std::max(
            0.0, DoubleToInteger(target_length->Number()) - arg_count));
      }
    }
    LookupIterator it(isolate, function, isolate->factory()->length_string(),
                      function);
    DCHECK_EQ(LookupIterator::ACCESSOR, it.state());
    RETURN_ON_EXCEPTION_VALUE(isolate,
                              JSObject::DefineOwnPropertyIgnoreAttributes(
                                  &it, length, it.property_attributes()),
                              Nothing<bool>());
  }

  // Setup the "name" property based on the "name" of the {target}.
  // If the target's name is the default JSFunction accessor, we can keep the
  // accessor that's installed by default on the
  // JSBoundFunction/JSWrappedFunction. It lazily computes the value from the
  // underlying internal name.
  Handle<AccessorInfo> function_name_accessor =
      isolate->factory()->function_name_accessor();
  LookupIterator name_lookup(isolate, target, isolate->factory()->name_string(),
                             target);
  if (!target->IsJSFunction() ||
      name_lookup.state() != LookupIterator::ACCESSOR ||
      !name_lookup.GetAccessors().is_identical_to(function_name_accessor) ||
      (name_lookup.IsFound() && !name_lookup.HolderIsReceiver())) {
    Handle<Object> target_name;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, target_name,
                                     Object::GetProperty(&name_lookup),
                                     Nothing<bool>());
    Handle<String> name;
    if (target_name->IsString()) {
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, name,
          Name::ToFunctionName(isolate, Handle<String>::cast(target_name)),
          Nothing<bool>());
      if (!prefix.is_null()) {
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate, name, isolate->factory()->NewConsString(prefix, name),
            Nothing<bool>());
      }
    } else if (prefix.is_null()) {
      name = isolate->factory()->empty_string();
    } else {
      name = prefix;
    }
    LookupIterator it(isolate, function, isolate->factory()->name_string());
    DCHECK_EQ(LookupIterator::ACCESSOR, it.state());
    RETURN_ON_EXCEPTION_VALUE(isolate,
                              JSObject::DefineOwnPropertyIgnoreAttributes(
                                  &it, name, it.property_attributes()),
                              Nothing<bool>());
  }

  return Just(true);
}

// static
MaybeHandle<String> JSBoundFunction::GetName(Isolate* isolate,
                                             Handle<JSBoundFunction> function) {
  Handle<String> prefix = isolate->factory()->bound__string();
  Handle<String> target_name = prefix;
  Factory* factory = isolate->factory();
  // Concatenate the "bound " up to the last non-bound target.
  while (function->bound_target_function().IsJSBoundFunction()) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, target_name,
                               factory->NewConsString(prefix, target_name),
                               String);
    function = handle(JSBoundFunction::cast(function->bound_target_function()),
                      isolate);
  }
  if (function->bound_target_function().IsJSWrappedFunction()) {
    Handle<JSWrappedFunction> target(
        JSWrappedFunction::cast(function->bound_target_function()), isolate);
    Handle<String> name;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, name, JSWrappedFunction::GetName(isolate, target), String);
    return factory->NewConsString(target_name, name);
  }
  if (function->bound_target_function().IsJSFunction()) {
    Handle<JSFunction> target(
        JSFunction::cast(function->bound_target_function()), isolate);
    Handle<String> name = JSFunction::GetName(isolate, target);
    return factory->NewConsString(target_name, name);
  }
  // This will omit the proper target name for bound JSProxies.
  return target_name;
}

// static
Maybe<int> JSBoundFunction::GetLength(Isolate* isolate,
                                      Handle<JSBoundFunction> function) {
  int nof_bound_arguments = function->bound_arguments().length();
  while (function->bound_target_function().IsJSBoundFunction()) {
    function = handle(JSBoundFunction::cast(function->bound_target_function()),
                      isolate);
    // Make sure we never overflow {nof_bound_arguments}, the number of
    // arguments of a function is strictly limited by the max length of an
    // JSAarray, Smi::kMaxValue is thus a reasonably good overestimate.
    int length = function->bound_arguments().length();
    if (V8_LIKELY(Smi::kMaxValue - nof_bound_arguments > length)) {
      nof_bound_arguments += length;
    } else {
      nof_bound_arguments = Smi::kMaxValue;
    }
  }
  if (function->bound_target_function().IsJSWrappedFunction()) {
    Handle<JSWrappedFunction> target(
        JSWrappedFunction::cast(function->bound_target_function()), isolate);
    int target_length = 0;
    MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, target_length, JSWrappedFunction::GetLength(isolate, target),
        Nothing<int>());
    int length = std::max(0, target_length - nof_bound_arguments);
    return Just(length);
  }
  // All non JSFunction targets get a direct property and don't use this
  // accessor.
  Handle<JSFunction> target(JSFunction::cast(function->bound_target_function()),
                            isolate);
  int target_length = target->length();

  int length = std::max(0, target_length - nof_bound_arguments);
  return Just(length);
}

// static
Handle<String> JSBoundFunction::ToString(Handle<JSBoundFunction> function) {
  Isolate* const isolate = function->GetIsolate();
  return isolate->factory()->function_native_code_string();
}

// static
MaybeHandle<String> JSWrappedFunction::GetName(
    Isolate* isolate, Handle<JSWrappedFunction> function) {
  STACK_CHECK(isolate, MaybeHandle<String>());
  Factory* factory = isolate->factory();
  Handle<String> target_name = factory->empty_string();
  Handle<JSReceiver> target =
      handle(function->wrapped_target_function(), isolate);
  if (target->IsJSBoundFunction()) {
    return JSBoundFunction::GetName(
        isolate,
        handle(JSBoundFunction::cast(function->wrapped_target_function()),
               isolate));
  } else if (target->IsJSFunction()) {
    return JSFunction::GetName(
        isolate,
        handle(JSFunction::cast(function->wrapped_target_function()), isolate));
  }
  // This will omit the proper target name for bound JSProxies.
  return target_name;
}

// static
Maybe<int> JSWrappedFunction::GetLength(Isolate* isolate,
                                        Handle<JSWrappedFunction> function) {
  STACK_CHECK(isolate, Nothing<int>());
  Handle<JSReceiver> target =
      handle(function->wrapped_target_function(), isolate);
  if (target->IsJSBoundFunction()) {
    return JSBoundFunction::GetLength(
        isolate,
        handle(JSBoundFunction::cast(function->wrapped_target_function()),
               isolate));
  }
  // All non JSFunction targets get a direct property and don't use this
  // accessor.
  return Just(Handle<JSFunction>::cast(target)->length());
}

// static
Handle<String> JSWrappedFunction::ToString(Handle<JSWrappedFunction> function) {
  Isolate* const isolate = function->GetIsolate();
  return isolate->factory()->function_native_code_string();
}

// static
MaybeHandle<Object> JSWrappedFunction::Create(
    Isolate* isolate, Handle<NativeContext> creation_context,
    Handle<JSReceiver> value) {
  // The value must be a callable according to the specification.
  DCHECK(value->IsCallable());
  // The intermediate wrapped functions are not user-visible. And calling a
  // wrapped function won't cause a side effect in the creation realm.
  // Unwrap here to avoid nested unwrapping at the call site.
  if (value->IsJSWrappedFunction()) {
    Handle<JSWrappedFunction> target_wrapped =
        Handle<JSWrappedFunction>::cast(value);
    value =
        Handle<JSReceiver>(target_wrapped->wrapped_target_function(), isolate);
  }

  // 1. Let internalSlotsList be the internal slots listed in Table 2, plus
  // [[Prototype]] and [[Extensible]].
  // 2. Let wrapped be ! MakeBasicObject(internalSlotsList).
  // 3. Set wrapped.[[Prototype]] to
  // callerRealm.[[Intrinsics]].[[%Function.prototype%]].
  // 4. Set wrapped.[[Call]] as described in 2.1.
  // 5. Set wrapped.[[WrappedTargetFunction]] to Target.
  // 6. Set wrapped.[[Realm]] to callerRealm.
  Handle<JSWrappedFunction> wrapped =
      isolate->factory()->NewJSWrappedFunction(creation_context, value);

  // 7. Let result be CopyNameAndLength(wrapped, Target, "wrapped").
  Maybe<bool> is_abrupt =
      JSFunctionOrBoundFunctionOrWrappedFunction::CopyNameAndLength(
          isolate, wrapped, value, Handle<String>(), 0);

  // 8. If result is an Abrupt Completion, throw a TypeError exception.
  if (is_abrupt.IsNothing()) {
    DCHECK(isolate->has_pending_exception());
    isolate->clear_pending_exception();
    // TODO(v8:11989): provide a non-observable inspection on the
    // pending_exception to the newly created TypeError.
    // https://github.com/tc39/proposal-shadowrealm/issues/353

    // The TypeError thrown is created with creation Realm's TypeError
    // constructor instead of the executing Realm's.
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate,
        NewError(Handle<JSFunction>(creation_context->type_error_function(),
                                    isolate),
                 MessageTemplate::kCannotWrap),
        {});
  }
  DCHECK(is_abrupt.FromJust());

  // 9. Return wrapped.
  return wrapped;
}

// static
Handle<String> JSFunction::GetName(Isolate* isolate,
                                   Handle<JSFunction> function) {
  if (function->shared().name_should_print_as_anonymous()) {
    return isolate->factory()->anonymous_string();
  }
  return handle(function->shared().Name(), isolate);
}

// static
void JSFunction::EnsureClosureFeedbackCellArray(
    Handle<JSFunction> function, bool reset_budget_for_feedback_allocation) {
  Isolate* const isolate = function->GetIsolate();
  DCHECK(function->shared().is_compiled());
  DCHECK(function->shared().HasFeedbackMetadata());
#if V8_ENABLE_WEBASSEMBLY
  if (function->shared().HasAsmWasmData()) return;
#endif  // V8_ENABLE_WEBASSEMBLY

  Handle<SharedFunctionInfo> shared(function->shared(), isolate);
  DCHECK(function->shared().HasBytecodeArray());

  const bool has_closure_feedback_cell_array =
      (function->has_closure_feedback_cell_array() ||
       function->has_feedback_vector());
  // Initialize the interrupt budget to the feedback vector allocation budget
  // when initializing the feedback cell for the first time or after a bytecode
  // flush. We retain the closure feedback cell array on bytecode flush, so
  // reset_budget_for_feedback_allocation is used to reset the budget in these
  // cases.
  if (reset_budget_for_feedback_allocation ||
      !has_closure_feedback_cell_array) {
    function->SetInterruptBudget(isolate);
  }

  if (has_closure_feedback_cell_array) {
    return;
  }

  Handle<HeapObject> feedback_cell_array =
      ClosureFeedbackCellArray::New(isolate, shared);
  // Many closure cell is used as a way to specify that there is no
  // feedback cell for this function and a new feedback cell has to be
  // allocated for this funciton. For ex: for eval functions, we have to create
  // a feedback cell and cache it along with the code. It is safe to use
  // many_closure_cell to indicate this because in regular cases, it should
  // already have a feedback_vector / feedback cell array allocated.
  if (function->raw_feedback_cell() == isolate->heap()->many_closures_cell()) {
    Handle<FeedbackCell> feedback_cell =
        isolate->factory()->NewOneClosureCell(feedback_cell_array);
    function->set_raw_feedback_cell(*feedback_cell, kReleaseStore);
    function->SetInterruptBudget(isolate);
  } else {
    function->raw_feedback_cell().set_value(*feedback_cell_array,
                                            kReleaseStore);
  }
}

// static
void JSFunction::EnsureFeedbackVector(Isolate* isolate,
                                      Handle<JSFunction> function,
                                      IsCompiledScope* compiled_scope) {
  DCHECK(compiled_scope->is_compiled());
  DCHECK(function->shared().HasFeedbackMetadata());
  if (function->has_feedback_vector()) return;
#if V8_ENABLE_WEBASSEMBLY
  if (function->shared().HasAsmWasmData()) return;
#endif  // V8_ENABLE_WEBASSEMBLY

  CreateAndAttachFeedbackVector(isolate, function, compiled_scope);
}

// static
void JSFunction::CreateAndAttachFeedbackVector(
    Isolate* isolate, Handle<JSFunction> function,
    IsCompiledScope* compiled_scope) {
  DCHECK(compiled_scope->is_compiled());
  DCHECK(function->shared().HasFeedbackMetadata());
  DCHECK(!function->has_feedback_vector());
#if V8_ENABLE_WEBASSEMBLY
  DCHECK(!function->shared().HasAsmWasmData());
#endif  // V8_ENABLE_WEBASSEMBLY

  Handle<SharedFunctionInfo> shared(function->shared(), isolate);
  DCHECK(function->shared().HasBytecodeArray());

  EnsureClosureFeedbackCellArray(function, false);
  Handle<ClosureFeedbackCellArray> closure_feedback_cell_array =
      handle(function->closure_feedback_cell_array(), isolate);
  Handle<FeedbackVector> feedback_vector = FeedbackVector::New(
      isolate, shared, closure_feedback_cell_array, compiled_scope);
  // EnsureClosureFeedbackCellArray should handle the special case where we need
  // to allocate a new feedback cell. Please look at comment in that function
  // for more details.
  DCHECK(function->raw_feedback_cell() !=
         isolate->heap()->many_closures_cell());
  function->raw_feedback_cell().set_value(*feedback_vector, kReleaseStore);
  function->SetInterruptBudget(isolate);

  DCHECK_EQ(v8_flags.log_function_events,
            feedback_vector->log_next_execution());
}

// static
void JSFunction::InitializeFeedbackCell(
    Handle<JSFunction> function, IsCompiledScope* is_compiled_scope,
    bool reset_budget_for_feedback_allocation) {
  Isolate* const isolate = function->GetIsolate();
#if V8_ENABLE_WEBASSEMBLY
  // The following checks ensure that the feedback vectors are compatible with
  // the feedback metadata. For Asm / Wasm functions we never allocate / use
  // feedback vectors, so a mismatch between the metadata and feedback vector is
  // harmless. The checks could fail for functions that has has_asm_wasm_broken
  // set at runtime (for ex: failed instantiation).
  if (function->shared().HasAsmWasmData()) return;
#endif  // V8_ENABLE_WEBASSEMBLY

  if (function->has_feedback_vector()) {
    CHECK_EQ(function->feedback_vector().length(),
             function->feedback_vector().metadata().slot_count());
    return;
  }

  if (function->has_closure_feedback_cell_array()) {
    CHECK_EQ(
        function->closure_feedback_cell_array().length(),
        function->shared().feedback_metadata().create_closure_slot_count());
  }

  const bool needs_feedback_vector =
      !v8_flags.lazy_feedback_allocation || v8_flags.always_turbofan ||
      // We also need a feedback vector for certain log events, collecting type
      // profile and more precise code coverage.
      v8_flags.log_function_events ||
      !isolate->is_best_effort_code_coverage() ||
      function->shared().sparkplug_compiled();

  if (needs_feedback_vector) {
    CreateAndAttachFeedbackVector(isolate, function, is_compiled_scope);
  } else {
    EnsureClosureFeedbackCellArray(function,
                                   reset_budget_for_feedback_allocation);
  }
  // TODO(jgruber): Unduplicate these conditions from tiering-manager.cc.
  if (function->shared().sparkplug_compiled() &&
      CanCompileWithBaseline(isolate, function->shared()) &&
      function->ActiveTierIsIgnition()) {
    if (v8_flags.baseline_batch_compilation) {
      isolate->baseline_batch_compiler()->EnqueueFunction(function);
    } else {
      IsCompiledScope is_compiled_scope(
          function->shared().is_compiled_scope(isolate));
      Compiler::CompileBaseline(isolate, function, Compiler::CLEAR_EXCEPTION,
                                &is_compiled_scope);
    }
  }
}

namespace {

void SetInstancePrototype(Isolate* isolate, Handle<JSFunction> function,
                          Handle<JSReceiver> value) {
  // Now some logic for the maps of the objects that are created by using this
  // function as a constructor.
  if (function->has_initial_map()) {
    // If the function has allocated the initial map replace it with a
    // copy containing the new prototype.  Also complete any in-object
    // slack tracking that is in progress at this point because it is
    // still tracking the old copy.
    function->CompleteInobjectSlackTrackingIfActive();

    Handle<Map> initial_map(function->initial_map(), isolate);

    if (!isolate->bootstrapper()->IsActive() &&
        initial_map->instance_type() == JS_OBJECT_TYPE) {
      // Put the value in the initial map field until an initial map is needed.
      // At that point, a new initial map is created and the prototype is put
      // into the initial map where it belongs.
      function->set_prototype_or_initial_map(*value, kReleaseStore);
    } else {
      Handle<Map> new_map =
          Map::Copy(isolate, initial_map, "SetInstancePrototype");
      JSFunction::SetInitialMap(isolate, function, new_map, value);
      DCHECK_IMPLIES(!isolate->bootstrapper()->IsActive(),
                     *function != function->native_context().array_function());
    }

    // Deoptimize all code that embeds the previous initial map.
    DependentCode::DeoptimizeDependencyGroups(
        isolate, *initial_map, DependentCode::kInitialMapChangedGroup);
  } else {
    // Put the value in the initial map field until an initial map is
    // needed.  At that point, a new initial map is created and the
    // prototype is put into the initial map where it belongs.
    function->set_prototype_or_initial_map(*value, kReleaseStore);
    if (value->IsJSObject()) {
      // Optimize as prototype to detach it from its transition tree.
      JSObject::OptimizeAsPrototype(Handle<JSObject>::cast(value));
    }
  }
}

}  // anonymous namespace

void JSFunction::SetPrototype(Handle<JSFunction> function,
                              Handle<Object> value) {
  DCHECK(function->IsConstructor() ||
         IsGeneratorFunction(function->shared().kind()));
  Isolate* isolate = function->GetIsolate();
  Handle<JSReceiver> construct_prototype;

  // If the value is not a JSReceiver, store the value in the map's
  // constructor field so it can be accessed.  Also, set the prototype
  // used for constructing objects to the original object prototype.
  // See ECMA-262 13.2.2.
  if (!value->IsJSReceiver()) {
    // Copy the map so this does not affect unrelated functions.
    // Remove map transitions because they point to maps with a
    // different prototype.
    Handle<Map> new_map =
        Map::Copy(isolate, handle(function->map(), isolate), "SetPrototype");

    new_map->SetConstructor(*value);
    new_map->set_has_non_instance_prototype(true);
    JSObject::MigrateToMap(isolate, function, new_map);

    FunctionKind kind = function->shared().kind();
    Handle<Context> native_context(function->native_context(), isolate);

    construct_prototype = Handle<JSReceiver>(
        IsGeneratorFunction(kind)
            ? IsAsyncFunction(kind)
                  ? native_context->initial_async_generator_prototype()
                  : native_context->initial_generator_prototype()
            : native_context->initial_object_prototype(),
        isolate);
  } else {
    construct_prototype = Handle<JSReceiver>::cast(value);
    function->map().set_has_non_instance_prototype(false);
  }

  SetInstancePrototype(isolate, function, construct_prototype);
}

void JSFunction::SetInitialMap(Isolate* isolate, Handle<JSFunction> function,
                               Handle<Map> map, Handle<HeapObject> prototype) {
  SetInitialMap(isolate, function, map, prototype, function);
}

void JSFunction::SetInitialMap(Isolate* isolate, Handle<JSFunction> function,
                               Handle<Map> map, Handle<HeapObject> prototype,
                               Handle<JSFunction> constructor) {
  if (map->prototype() != *prototype) {
    Map::SetPrototype(isolate, map, prototype);
  }
  map->SetConstructor(*constructor);
  function->set_prototype_or_initial_map(*map, kReleaseStore);
  if (v8_flags.log_maps) {
    LOG(isolate, MapEvent("InitialMap", Handle<Map>(), map, "",
                          SharedFunctionInfo::DebugName(
                              handle(function->shared(), isolate))));
  }
}

void JSFunction::EnsureHasInitialMap(Handle<JSFunction> function) {
  DCHECK(function->has_prototype_slot());
  DCHECK(function->IsConstructor() ||
         IsResumableFunction(function->shared().kind()));
  if (function->has_initial_map()) return;
  Isolate* isolate = function->GetIsolate();

  int expected_nof_properties =
      CalculateExpectedNofProperties(isolate, function);

  // {CalculateExpectedNofProperties} can have had the side effect of creating
  // the initial map (e.g. it could have triggered an optimized compilation
  // whose dependency installation reentered {EnsureHasInitialMap}).
  if (function->has_initial_map()) return;

  // Create a new map with the size and number of in-object properties suggested
  // by the function.
  InstanceType instance_type;
  if (IsResumableFunction(function->shared().kind())) {
    instance_type = IsAsyncGeneratorFunction(function->shared().kind())
                        ? JS_ASYNC_GENERATOR_OBJECT_TYPE
                        : JS_GENERATOR_OBJECT_TYPE;
  } else {
    instance_type = JS_OBJECT_TYPE;
  }

  int instance_size;
  int inobject_properties;
  CalculateInstanceSizeHelper(instance_type, false, 0, expected_nof_properties,
                              &instance_size, &inobject_properties);

  Handle<Map> map = isolate->factory()->NewMap(instance_type, instance_size,
                                               TERMINAL_FAST_ELEMENTS_KIND,
                                               inobject_properties);

  // Fetch or allocate prototype.
  Handle<HeapObject> prototype;
  if (function->has_instance_prototype()) {
    prototype = handle(function->instance_prototype(), isolate);
  } else {
    prototype = isolate->factory()->NewFunctionPrototype(function);
  }
  DCHECK(map->has_fast_object_elements());

  // Finally link initial map and constructor function.
  DCHECK(prototype->IsJSReceiver());
  JSFunction::SetInitialMap(isolate, function, map, prototype);
  map->StartInobjectSlackTracking();
}

namespace {

#ifdef DEBUG
bool CanSubclassHaveInobjectProperties(InstanceType instance_type) {
  switch (instance_type) {
    case JS_API_OBJECT_TYPE:
    case JS_ARRAY_BUFFER_TYPE:
    case JS_ARRAY_ITERATOR_PROTOTYPE_TYPE:
    case JS_ARRAY_TYPE:
    case JS_ASYNC_FROM_SYNC_ITERATOR_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_DATA_VIEW_TYPE:
    case JS_DATE_TYPE:
    case JS_GENERATOR_OBJECT_TYPE:
    case JS_FUNCTION_TYPE:
    case JS_CLASS_CONSTRUCTOR_TYPE:
    case JS_PROMISE_CONSTRUCTOR_TYPE:
    case JS_REG_EXP_CONSTRUCTOR_TYPE:
    case JS_ARRAY_CONSTRUCTOR_TYPE:
#define TYPED_ARRAY_CONSTRUCTORS_SWITCH(Type, type, TYPE, Ctype) \
  case TYPE##_TYPED_ARRAY_CONSTRUCTOR_TYPE:
      TYPED_ARRAYS(TYPED_ARRAY_CONSTRUCTORS_SWITCH)
#undef TYPED_ARRAY_CONSTRUCTORS_SWITCH
    case JS_ITERATOR_PROTOTYPE_TYPE:
    case JS_MAP_ITERATOR_PROTOTYPE_TYPE:
    case JS_OBJECT_PROTOTYPE_TYPE:
    case JS_PROMISE_PROTOTYPE_TYPE:
    case JS_REG_EXP_PROTOTYPE_TYPE:
    case JS_SET_ITERATOR_PROTOTYPE_TYPE:
    case JS_SET_PROTOTYPE_TYPE:
    case JS_STRING_ITERATOR_PROTOTYPE_TYPE:
    case JS_TYPED_ARRAY_PROTOTYPE_TYPE:
#ifdef V8_INTL_SUPPORT
    case JS_COLLATOR_TYPE:
    case JS_DATE_TIME_FORMAT_TYPE:
    case JS_DISPLAY_NAMES_TYPE:
    case JS_DURATION_FORMAT_TYPE:
    case JS_LIST_FORMAT_TYPE:
    case JS_LOCALE_TYPE:
    case JS_NUMBER_FORMAT_TYPE:
    case JS_PLURAL_RULES_TYPE:
    case JS_RELATIVE_TIME_FORMAT_TYPE:
    case JS_SEGMENT_ITERATOR_TYPE:
    case JS_SEGMENTER_TYPE:
    case JS_SEGMENTS_TYPE:
    case JS_V8_BREAK_ITERATOR_TYPE:
#endif
    case JS_ASYNC_FUNCTION_OBJECT_TYPE:
    case JS_ASYNC_GENERATOR_OBJECT_TYPE:
    case JS_MAP_TYPE:
    case JS_MESSAGE_OBJECT_TYPE:
    case JS_OBJECT_TYPE:
    case JS_ERROR_TYPE:
    case JS_FINALIZATION_REGISTRY_TYPE:
    case JS_ARGUMENTS_OBJECT_TYPE:
    case JS_PROMISE_TYPE:
    case JS_REG_EXP_TYPE:
    case JS_SET_TYPE:
    case JS_SHADOW_REALM_TYPE:
    case JS_SPECIAL_API_OBJECT_TYPE:
    case JS_TYPED_ARRAY_TYPE:
    case JS_PRIMITIVE_WRAPPER_TYPE:
    case JS_TEMPORAL_CALENDAR_TYPE:
    case JS_TEMPORAL_DURATION_TYPE:
    case JS_TEMPORAL_INSTANT_TYPE:
    case JS_TEMPORAL_PLAIN_DATE_TYPE:
    case JS_TEMPORAL_PLAIN_DATE_TIME_TYPE:
    case JS_TEMPORAL_PLAIN_MONTH_DAY_TYPE:
    case JS_TEMPORAL_PLAIN_TIME_TYPE:
    case JS_TEMPORAL_PLAIN_YEAR_MONTH_TYPE:
    case JS_TEMPORAL_TIME_ZONE_TYPE:
    case JS_TEMPORAL_ZONED_DATE_TIME_TYPE:
    case JS_WEAK_MAP_TYPE:
    case JS_WEAK_REF_TYPE:
    case JS_WEAK_SET_TYPE:
#if V8_ENABLE_WEBASSEMBLY
    case WASM_GLOBAL_OBJECT_TYPE:
    case WASM_INSTANCE_OBJECT_TYPE:
    case WASM_MEMORY_OBJECT_TYPE:
    case WASM_MODULE_OBJECT_TYPE:
    case WASM_TABLE_OBJECT_TYPE:
    case WASM_VALUE_OBJECT_TYPE:
#endif  // V8_ENABLE_WEBASSEMBLY
      return true;

    case BIGINT_TYPE:
    case OBJECT_BOILERPLATE_DESCRIPTION_TYPE:
    case BYTECODE_ARRAY_TYPE:
    case BYTE_ARRAY_TYPE:
    case CELL_TYPE:
    case CODE_TYPE:
    case FILLER_TYPE:
    case FIXED_ARRAY_TYPE:
    case SCRIPT_CONTEXT_TABLE_TYPE:
    case FIXED_DOUBLE_ARRAY_TYPE:
    case FEEDBACK_METADATA_TYPE:
    case FOREIGN_TYPE:
    case FREE_SPACE_TYPE:
    case HASH_TABLE_TYPE:
    case ORDERED_HASH_MAP_TYPE:
    case ORDERED_HASH_SET_TYPE:
    case ORDERED_NAME_DICTIONARY_TYPE:
    case NAME_DICTIONARY_TYPE:
    case GLOBAL_DICTIONARY_TYPE:
    case NUMBER_DICTIONARY_TYPE:
    case SIMPLE_NUMBER_DICTIONARY_TYPE:
    case HEAP_NUMBER_TYPE:
    case JS_BOUND_FUNCTION_TYPE:
    case JS_GLOBAL_OBJECT_TYPE:
    case JS_GLOBAL_PROXY_TYPE:
    case JS_PROXY_TYPE:
    case JS_WRAPPED_FUNCTION_TYPE:
    case MAP_TYPE:
    case ODDBALL_TYPE:
    case PROPERTY_CELL_TYPE:
    case SHARED_FUNCTION_INFO_TYPE:
    case SYMBOL_TYPE:
    case ALLOCATION_SITE_TYPE:

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case FIXED_##TYPE##_ARRAY_TYPE:
#undef TYPED_ARRAY_CASE

#define MAKE_STRUCT_CASE(TYPE, Name, name) case TYPE:
      STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
      // We must not end up here for these instance types at all.
      UNREACHABLE();
    // Fall through.
    default:
      return false;
  }
}
#endif  // DEBUG

bool FastInitializeDerivedMap(Isolate* isolate, Handle<JSFunction> new_target,
                              Handle<JSFunction> constructor,
                              Handle<Map> constructor_initial_map) {
  // Use the default intrinsic prototype instead.
  if (!new_target->has_prototype_slot()) return false;
  // Check that |function|'s initial map still in sync with the |constructor|,
  // otherwise we must create a new initial map for |function|.
  if (new_target->has_initial_map() &&
      new_target->initial_map().GetConstructor() == *constructor) {
    DCHECK(new_target->instance_prototype().IsJSReceiver());
    return true;
  }
  InstanceType instance_type = constructor_initial_map->instance_type();
  DCHECK(CanSubclassHaveInobjectProperties(instance_type));
  // Create a new map with the size and number of in-object properties
  // suggested by |function|.

  // Link initial map and constructor function if the new.target is actually a
  // subclass constructor.
  if (!IsDerivedConstructor(new_target->shared().kind())) return false;

  int instance_size;
  int in_object_properties;
  int embedder_fields =
      JSObject::GetEmbedderFieldCount(*constructor_initial_map);
  // Constructor expects certain number of in-object properties to be in the
  // object. However, CalculateExpectedNofProperties() may return smaller value
  // if 1) the constructor is not in the prototype chain of new_target, or
  // 2) the prototype chain is modified during iteration, or 3) compilation
  // failure occur during prototype chain iteration.
  // So we take the maximum of two values.
  int expected_nof_properties = std::max(
      static_cast<int>(constructor->shared().expected_nof_properties()),
      JSFunction::CalculateExpectedNofProperties(isolate, new_target));
  JSFunction::CalculateInstanceSizeHelper(
      instance_type, constructor_initial_map->has_prototype_slot(),
      embedder_fields, expected_nof_properties, &instance_size,
      &in_object_properties);

  int pre_allocated = constructor_initial_map->GetInObjectProperties() -
                      constructor_initial_map->UnusedPropertyFields();
  CHECK_LE(constructor_initial_map->UsedInstanceSize(), instance_size);
  int unused_property_fields = in_object_properties - pre_allocated;
  Handle<Map> map =
      Map::CopyInitialMap(isolate, constructor_initial_map, instance_size,
                          in_object_properties, unused_property_fields);
  map->set_new_target_is_base(false);
  Handle<HeapObject> prototype(new_target->instance_prototype(), isolate);
  JSFunction::SetInitialMap(isolate, new_target, map, prototype, constructor);
  DCHECK(new_target->instance_prototype().IsJSReceiver());
  map->set_construction_counter(Map::kNoSlackTracking);
  map->StartInobjectSlackTracking();
  return true;
}

}  // namespace

// static
MaybeHandle<Map> JSFunction::GetDerivedMap(Isolate* isolate,
                                           Handle<JSFunction> constructor,
                                           Handle<JSReceiver> new_target) {
  EnsureHasInitialMap(constructor);

  Handle<Map> constructor_initial_map(constructor->initial_map(), isolate);
  if (*new_target == *constructor) return constructor_initial_map;

  Handle<Map> result_map;
  // Fast case, new.target is a subclass of constructor. The map is cacheable
  // (and may already have been cached). new.target.prototype is guaranteed to
  // be a JSReceiver.
  if (new_target->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(new_target);
    if (FastInitializeDerivedMap(isolate, function, constructor,
                                 constructor_initial_map)) {
      return handle(function->initial_map(), isolate);
    }
  }

  // Slow path, new.target is either a proxy or can't cache the map.
  // new.target.prototype is not guaranteed to be a JSReceiver, and may need to
  // fall back to the intrinsicDefaultProto.
  Handle<Object> prototype;
  if (new_target->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(new_target);
    if (function->has_prototype_slot()) {
      // Make sure the new.target.prototype is cached.
      EnsureHasInitialMap(function);
      prototype = handle(function->prototype(), isolate);
    } else {
      // No prototype property, use the intrinsict default proto further down.
      prototype = isolate->factory()->undefined_value();
    }
  } else {
    Handle<String> prototype_string = isolate->factory()->prototype_string();
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, prototype,
        JSReceiver::GetProperty(isolate, new_target, prototype_string), Map);
    // The above prototype lookup might change the constructor and its
    // prototype, hence we have to reload the initial map.
    EnsureHasInitialMap(constructor);
    constructor_initial_map = handle(constructor->initial_map(), isolate);
  }

  // If prototype is not a JSReceiver, fetch the intrinsicDefaultProto from the
  // correct realm. Rather than directly fetching the .prototype, we fetch the
  // constructor that points to the .prototype. This relies on
  // constructor.prototype being FROZEN for those constructors.
  if (!prototype->IsJSReceiver()) {
    Handle<Context> context;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, context,
                               JSReceiver::GetFunctionRealm(new_target), Map);
    DCHECK(context->IsNativeContext());
    Handle<Object> maybe_index = JSReceiver::GetDataProperty(
        isolate, constructor,
        isolate->factory()->native_context_index_symbol());
    int index = maybe_index->IsSmi() ? Smi::ToInt(*maybe_index)
                                     : Context::OBJECT_FUNCTION_INDEX;
    Handle<JSFunction> realm_constructor(JSFunction::cast(context->get(index)),
                                         isolate);
    prototype = handle(realm_constructor->prototype(), isolate);
  }

  Handle<Map> map = Map::CopyInitialMap(isolate, constructor_initial_map);
  map->set_new_target_is_base(false);
  CHECK(prototype->IsJSReceiver());
  if (map->prototype() != *prototype)
    Map::SetPrototype(isolate, map, Handle<HeapObject>::cast(prototype));
  map->SetConstructor(*constructor);
  return map;
}

namespace {

// Assert that the computations in TypedArrayElementsKindToConstructorIndex and
// TypedArrayElementsKindToRabGsabCtorIndex are sound.
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype)                         \
  static_assert(Context::TYPE##_ARRAY_FUN_INDEX ==                        \
                Context::FIRST_FIXED_TYPED_ARRAY_FUN_INDEX +              \
                    ElementsKind::TYPE##_ELEMENTS -                       \
                    ElementsKind::FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND); \
  static_assert(Context::RAB_GSAB_##TYPE##_ARRAY_MAP_INDEX ==             \
                Context::FIRST_RAB_GSAB_TYPED_ARRAY_MAP_INDEX +           \
                    ElementsKind::TYPE##_ELEMENTS -                       \
                    ElementsKind::FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND);

TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

int TypedArrayElementsKindToConstructorIndex(ElementsKind elements_kind) {
  return Context::FIRST_FIXED_TYPED_ARRAY_FUN_INDEX + elements_kind -
         ElementsKind::FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND;
}

int TypedArrayElementsKindToRabGsabCtorIndex(ElementsKind elements_kind) {
  return Context::FIRST_RAB_GSAB_TYPED_ARRAY_MAP_INDEX + elements_kind -
         ElementsKind::FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND;
}

}  // namespace

MaybeHandle<Map> JSFunction::GetDerivedRabGsabMap(
    Isolate* isolate, Handle<JSFunction> constructor,
    Handle<JSReceiver> new_target) {
  MaybeHandle<Map> maybe_map = GetDerivedMap(isolate, constructor, new_target);
  Handle<Map> map;
  if (!maybe_map.ToHandle(&map)) {
    return MaybeHandle<Map>();
  }
  {
    DisallowHeapAllocation no_alloc;
    NativeContext context = isolate->context().native_context();
    int ctor_index =
        TypedArrayElementsKindToConstructorIndex(map->elements_kind());
    if (*new_target == context.get(ctor_index)) {
      ctor_index =
          TypedArrayElementsKindToRabGsabCtorIndex(map->elements_kind());
      return handle(Map::cast(context.get(ctor_index)), isolate);
    }
  }

  // This only happens when subclassing TypedArrays. Create a new map with the
  // corresponding RAB / GSAB ElementsKind. Note: the map is not cached and
  // reused -> every array gets a unique map, making ICs slow.
  Handle<Map> rab_gsab_map = Map::Copy(isolate, map, "RAB / GSAB");
  rab_gsab_map->set_elements_kind(
      GetCorrespondingRabGsabElementsKind(map->elements_kind()));
  return rab_gsab_map;
}

int JSFunction::ComputeInstanceSizeWithMinSlack(Isolate* isolate) {
  CHECK(has_initial_map());
  if (initial_map().IsInobjectSlackTrackingInProgress()) {
    int slack = initial_map().ComputeMinObjectSlack(isolate);
    return initial_map().InstanceSizeFromSlack(slack);
  }
  return initial_map().instance_size();
}

std::unique_ptr<char[]> JSFunction::DebugNameCStr() {
  return shared().DebugNameCStr();
}

void JSFunction::PrintName(FILE* out) {
  PrintF(out, "%s", DebugNameCStr().get());
}

namespace {

bool UseFastFunctionNameLookup(Isolate* isolate, Map map) {
  DCHECK(map.IsJSFunctionMap());
  if (map.NumberOfOwnDescriptors() <
      JSFunction::kMinDescriptorsForFastBindAndWrap) {
    return false;
  }
  DCHECK(!map.is_dictionary_map());
  HeapObject value;
  ReadOnlyRoots roots(isolate);
  auto descriptors = map.instance_descriptors(isolate);
  InternalIndex kNameIndex{JSFunction::kNameDescriptorIndex};
  if (descriptors.GetKey(kNameIndex) != roots.name_string() ||
      !descriptors.GetValue(kNameIndex)
           .GetHeapObjectIfStrong(isolate, &value)) {
    return false;
  }
  return value.IsAccessorInfo();
}

}  // namespace

Handle<String> JSFunction::GetDebugName(Handle<JSFunction> function) {
  // Below we use the same fast-path that we already established for
  // Function.prototype.bind(), where we avoid a slow "name" property
  // lookup if the DescriptorArray for the |function| still has the
  // "name" property at the original spot and that property is still
  // implemented via an AccessorInfo (which effectively means that
  // it must be the FunctionNameGetter).
  Isolate* isolate = function->GetIsolate();
  if (!UseFastFunctionNameLookup(isolate, function->map())) {
    // Normally there should be an else case for the fast-path check
    // above, which should invoke JSFunction::GetName(), since that's
    // what the FunctionNameGetter does, however GetDataProperty() has
    // never invoked accessors and thus always returned undefined for
    // JSFunction where the "name" property is untouched, so we retain
    // that exact behavior and go with SharedFunctionInfo::DebugName()
    // in case of the fast-path.
    Handle<Object> name =
        GetDataProperty(isolate, function, isolate->factory()->name_string());
    if (name->IsString()) return Handle<String>::cast(name);
  }
  return SharedFunctionInfo::DebugName(handle(function->shared(), isolate));
}

bool JSFunction::SetName(Handle<JSFunction> function, Handle<Name> name,
                         Handle<String> prefix) {
  Isolate* isolate = function->GetIsolate();
  Handle<String> function_name;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, function_name,
                                   Name::ToFunctionName(isolate, name), false);
  if (prefix->length() > 0) {
    IncrementalStringBuilder builder(isolate);
    builder.AppendString(prefix);
    builder.AppendCharacter(' ');
    builder.AppendString(function_name);
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, function_name, builder.Finish(),
                                     false);
  }
  RETURN_ON_EXCEPTION_VALUE(
      isolate,
      JSObject::DefinePropertyOrElementIgnoreAttributes(
          function, isolate->factory()->name_string(), function_name,
          static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY)),
      false);
  return true;
}

namespace {

Handle<String> NativeCodeFunctionSourceString(
    Handle<SharedFunctionInfo> shared_info) {
  Isolate* const isolate = shared_info->GetIsolate();
  IncrementalStringBuilder builder(isolate);
  builder.AppendCStringLiteral("function ");
  builder.AppendString(handle(shared_info->Name(), isolate));
  builder.AppendCStringLiteral("() { [native code] }");
  return builder.Finish().ToHandleChecked();
}

}  // namespace

// static
Handle<String> JSFunction::ToString(Handle<JSFunction> function) {
  Isolate* const isolate = function->GetIsolate();
  Handle<SharedFunctionInfo> shared_info(function->shared(), isolate);

  // Check if {function} should hide its source code.
  if (!shared_info->IsUserJavaScript()) {
    return NativeCodeFunctionSourceString(shared_info);
  }

  // Check if we should print {function} as a class.
  Handle<Object> maybe_class_positions = JSReceiver::GetDataProperty(
      isolate, function, isolate->factory()->class_positions_symbol());
  if (maybe_class_positions->IsClassPositions()) {
    ClassPositions class_positions =
        ClassPositions::cast(*maybe_class_positions);
    int start_position = class_positions.start();
    int end_position = class_positions.end();
    Handle<String> script_source(
        String::cast(Script::cast(shared_info->script()).source()), isolate);
    return replayio::RecordReplayStringHandle(
      "JSFunction::ToString",
      isolate,
      isolate->factory()->NewSubString(script_source, start_position, end_position)
    );
  }

  // Check if we have source code for the {function}.
  if (!shared_info->HasSourceCode()) {
    return NativeCodeFunctionSourceString(shared_info);
  }

  // If this function was compiled from asm.js, use the recorded offset
  // information.
#if V8_ENABLE_WEBASSEMBLY
  if (shared_info->HasWasmExportedFunctionData()) {
    Handle<WasmExportedFunctionData> function_data(
        shared_info->wasm_exported_function_data(), isolate);
    const wasm::WasmModule* module = function_data->instance().module();
    if (is_asmjs_module(module)) {
      std::pair<int, int> offsets =
          module->asm_js_offset_information->GetFunctionOffsets(
              declared_function_index(module, function_data->function_index()));
      Handle<String> source(
          String::cast(Script::cast(shared_info->script()).source()), isolate);
      return isolate->factory()->NewSubString(source, offsets.first,
                                              offsets.second);
    }
  }
#endif  // V8_ENABLE_WEBASSEMBLY

  if (shared_info->function_token_position() == kNoSourcePosition) {
    // If the function token position isn't valid, return [native code] to
    // ensure calling eval on the returned source code throws rather than
    // giving inconsistent call behaviour.
    isolate->CountUsage(
        v8::Isolate::UseCounterFeature::kFunctionTokenOffsetTooLongForToString);
    return NativeCodeFunctionSourceString(shared_info);
  }
  // [PRO-1304] Replay stringified content of the function
  return replayio::RecordReplayStringHandle(
    "JSFunction::ToString",
    isolate,
    Handle<String>::cast(SharedFunctionInfo::GetSourceCodeHarmony(shared_info))
  );
}

// static
int JSFunction::CalculateExpectedNofProperties(Isolate* isolate,
                                               Handle<JSFunction> function) {
  int expected_nof_properties = 0;
  for (PrototypeIterator iter(isolate, function, kStartAtReceiver);
       !iter.IsAtEnd(); iter.Advance()) {
    Handle<JSReceiver> current =
        PrototypeIterator::GetCurrent<JSReceiver>(iter);
    if (!current->IsJSFunction()) break;
    Handle<JSFunction> func = Handle<JSFunction>::cast(current);
    // The super constructor should be compiled for the number of expected
    // properties to be available.
    Handle<SharedFunctionInfo> shared(func->shared(), isolate);
    IsCompiledScope is_compiled_scope(shared->is_compiled_scope(isolate));
    if (is_compiled_scope.is_compiled() ||
        Compiler::Compile(isolate, func, Compiler::CLEAR_EXCEPTION,
                          &is_compiled_scope)) {
      DCHECK(shared->is_compiled());
      int count = shared->expected_nof_properties();
      // Check that the estimate is sensible.
      if (expected_nof_properties <= JSObject::kMaxInObjectProperties - count) {
        expected_nof_properties += count;
      } else {
        return JSObject::kMaxInObjectProperties;
      }
    } else {
      // In case there was a compilation error proceed iterating in case there
      // will be a builtin function in the prototype chain that requires
      // certain number of in-object properties.
      continue;
    }
  }
  // Inobject slack tracking will reclaim redundant inobject space
  // later, so we can afford to adjust the estimate generously,
  // meaning we over-allocate by at least 8 slots in the beginning.
  if (expected_nof_properties > 0) {
    expected_nof_properties += 8;
    if (expected_nof_properties > JSObject::kMaxInObjectProperties) {
      expected_nof_properties = JSObject::kMaxInObjectProperties;
    }
  }
  return expected_nof_properties;
}

// static
void JSFunction::CalculateInstanceSizeHelper(InstanceType instance_type,
                                             bool has_prototype_slot,
                                             int requested_embedder_fields,
                                             int requested_in_object_properties,
                                             int* instance_size,
                                             int* in_object_properties) {
  DCHECK_LE(static_cast<unsigned>(requested_embedder_fields),
            JSObject::kMaxEmbedderFields);
  int header_size = JSObject::GetHeaderSize(instance_type, has_prototype_slot);
  requested_embedder_fields *= kEmbedderDataSlotSizeInTaggedSlots;

  int max_nof_fields =
      (JSObject::kMaxInstanceSize - header_size) >> kTaggedSizeLog2;
  CHECK_LE(max_nof_fields, JSObject::kMaxInObjectProperties);
  CHECK_LE(static_cast<unsigned>(requested_embedder_fields),
           static_cast<unsigned>(max_nof_fields));
  *in_object_properties = std::min(requested_in_object_properties,
                                   max_nof_fields - requested_embedder_fields);
  *instance_size =
      header_size +
      ((requested_embedder_fields + *in_object_properties) << kTaggedSizeLog2);
  CHECK_EQ(*in_object_properties,
           ((*instance_size - header_size) >> kTaggedSizeLog2) -
               requested_embedder_fields);
  CHECK_LE(static_cast<unsigned>(*instance_size),
           static_cast<unsigned>(JSObject::kMaxInstanceSize));
}

void JSFunction::ClearAllTypeFeedbackInfoForTesting() {
  ResetIfCodeFlushed();
  if (has_feedback_vector()) {
    FeedbackVector vector = feedback_vector();
    Isolate* isolate = GetIsolate();
    if (vector.ClearAllSlotsForTesting(isolate)) {
      IC::OnFeedbackChanged(isolate, vector, FeedbackSlot::Invalid(),
                            "ClearAllTypeFeedbackInfoForTesting");
    }
  }
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"
