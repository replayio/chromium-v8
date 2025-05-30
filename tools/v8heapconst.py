#!/usr/bin/env python3
# Copyright 2019 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

# This file is automatically generated by mkgrokdump and should not
# be modified manually.

# List of known V8 instance types.
# yapf: disable

INSTANCE_TYPES = {
  0: "INTERNALIZED_STRING_TYPE",
  2: "EXTERNAL_INTERNALIZED_STRING_TYPE",
  8: "ONE_BYTE_INTERNALIZED_STRING_TYPE",
  10: "EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE",
  18: "UNCACHED_EXTERNAL_INTERNALIZED_STRING_TYPE",
  26: "UNCACHED_EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE",
  32: "STRING_TYPE",
  33: "CONS_STRING_TYPE",
  34: "EXTERNAL_STRING_TYPE",
  35: "SLICED_STRING_TYPE",
  37: "THIN_STRING_TYPE",
  40: "ONE_BYTE_STRING_TYPE",
  41: "CONS_ONE_BYTE_STRING_TYPE",
  42: "EXTERNAL_ONE_BYTE_STRING_TYPE",
  43: "SLICED_ONE_BYTE_STRING_TYPE",
  45: "THIN_ONE_BYTE_STRING_TYPE",
  50: "UNCACHED_EXTERNAL_STRING_TYPE",
  58: "UNCACHED_EXTERNAL_ONE_BYTE_STRING_TYPE",
  96: "SHARED_STRING_TYPE",
  98: "SHARED_EXTERNAL_STRING_TYPE",
  101: "SHARED_THIN_STRING_TYPE",
  104: "SHARED_ONE_BYTE_STRING_TYPE",
  106: "SHARED_EXTERNAL_ONE_BYTE_STRING_TYPE",
  109: "SHARED_THIN_ONE_BYTE_STRING_TYPE",
  114: "SHARED_UNCACHED_EXTERNAL_STRING_TYPE",
  122: "SHARED_UNCACHED_EXTERNAL_ONE_BYTE_STRING_TYPE",
  128: "SYMBOL_TYPE",
  129: "BIG_INT_BASE_TYPE",
  130: "HEAP_NUMBER_TYPE",
  131: "ODDBALL_TYPE",
  132: "PROMISE_FULFILL_REACTION_JOB_TASK_TYPE",
  133: "PROMISE_REJECT_REACTION_JOB_TASK_TYPE",
  134: "CALLABLE_TASK_TYPE",
  135: "CALLBACK_TASK_TYPE",
  136: "PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE",
  137: "LOAD_HANDLER_TYPE",
  138: "STORE_HANDLER_TYPE",
  139: "FUNCTION_TEMPLATE_INFO_TYPE",
  140: "OBJECT_TEMPLATE_INFO_TYPE",
  141: "ACCESS_CHECK_INFO_TYPE",
  142: "ACCESSOR_PAIR_TYPE",
  143: "ALIASED_ARGUMENTS_ENTRY_TYPE",
  144: "ALLOCATION_MEMENTO_TYPE",
  145: "ALLOCATION_SITE_TYPE",
  146: "ARRAY_BOILERPLATE_DESCRIPTION_TYPE",
  147: "ASM_WASM_DATA_TYPE",
  148: "ASYNC_GENERATOR_REQUEST_TYPE",
  149: "BREAK_POINT_TYPE",
  150: "BREAK_POINT_INFO_TYPE",
  151: "CACHED_TEMPLATE_OBJECT_TYPE",
  152: "CALL_SITE_INFO_TYPE",
  153: "CLASS_POSITIONS_TYPE",
  154: "DEBUG_INFO_TYPE",
  155: "ENUM_CACHE_TYPE",
  156: "ERROR_STACK_DATA_TYPE",
  157: "FEEDBACK_CELL_TYPE",
  158: "FUNCTION_TEMPLATE_RARE_DATA_TYPE",
  159: "INTERCEPTOR_INFO_TYPE",
  160: "INTERPRETER_DATA_TYPE",
  161: "MODULE_REQUEST_TYPE",
  162: "PROMISE_CAPABILITY_TYPE",
  163: "PROMISE_ON_STACK_TYPE",
  164: "PROMISE_REACTION_TYPE",
  165: "PROPERTY_DESCRIPTOR_OBJECT_TYPE",
  166: "PROTOTYPE_INFO_TYPE",
  167: "REG_EXP_BOILERPLATE_DESCRIPTION_TYPE",
  168: "SCRIPT_TYPE",
  169: "SCRIPT_OR_MODULE_TYPE",
  170: "SOURCE_TEXT_MODULE_INFO_ENTRY_TYPE",
  171: "STACK_FRAME_INFO_TYPE",
  172: "TEMPLATE_OBJECT_DESCRIPTION_TYPE",
  173: "TUPLE2_TYPE",
  174: "WASM_EXCEPTION_TAG_TYPE",
  175: "WASM_INDIRECT_FUNCTION_TABLE_TYPE",
  176: "FIXED_ARRAY_TYPE",
  177: "HASH_TABLE_TYPE",
  178: "EPHEMERON_HASH_TABLE_TYPE",
  179: "GLOBAL_DICTIONARY_TYPE",
  180: "NAME_DICTIONARY_TYPE",
  181: "NAME_TO_INDEX_HASH_TABLE_TYPE",
  182: "NUMBER_DICTIONARY_TYPE",
  183: "ORDERED_HASH_MAP_TYPE",
  184: "ORDERED_HASH_SET_TYPE",
  185: "ORDERED_NAME_DICTIONARY_TYPE",
  186: "REGISTERED_SYMBOL_TABLE_TYPE",
  187: "SIMPLE_NUMBER_DICTIONARY_TYPE",
  188: "CLOSURE_FEEDBACK_CELL_ARRAY_TYPE",
  189: "OBJECT_BOILERPLATE_DESCRIPTION_TYPE",
  190: "SCRIPT_CONTEXT_TABLE_TYPE",
  191: "BYTE_ARRAY_TYPE",
  192: "BYTECODE_ARRAY_TYPE",
  193: "FIXED_DOUBLE_ARRAY_TYPE",
  194: "INTERNAL_CLASS_WITH_SMI_ELEMENTS_TYPE",
  195: "SLOPPY_ARGUMENTS_ELEMENTS_TYPE",
  196: "TURBOFAN_BITSET_TYPE_TYPE",
  197: "TURBOFAN_HEAP_CONSTANT_TYPE_TYPE",
  198: "TURBOFAN_OTHER_NUMBER_CONSTANT_TYPE_TYPE",
  199: "TURBOFAN_RANGE_TYPE_TYPE",
  200: "TURBOFAN_UNION_TYPE_TYPE",
  201: "EXPORTED_SUB_CLASS_BASE_TYPE",
  202: "EXPORTED_SUB_CLASS_TYPE",
  203: "EXPORTED_SUB_CLASS2_TYPE",
  204: "FOREIGN_TYPE",
  205: "AWAIT_CONTEXT_TYPE",
  206: "BLOCK_CONTEXT_TYPE",
  207: "CATCH_CONTEXT_TYPE",
  208: "DEBUG_EVALUATE_CONTEXT_TYPE",
  209: "EVAL_CONTEXT_TYPE",
  210: "FUNCTION_CONTEXT_TYPE",
  211: "MODULE_CONTEXT_TYPE",
  212: "NATIVE_CONTEXT_TYPE",
  213: "SCRIPT_CONTEXT_TYPE",
  214: "WITH_CONTEXT_TYPE",
  215: "UNCOMPILED_DATA_WITH_PREPARSE_DATA_TYPE",
  216: "UNCOMPILED_DATA_WITH_PREPARSE_DATA_AND_JOB_TYPE",
  217: "UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_TYPE",
  218: "UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_WITH_JOB_TYPE",
  219: "WASM_FUNCTION_DATA_TYPE",
  220: "WASM_CAPI_FUNCTION_DATA_TYPE",
  221: "WASM_EXPORTED_FUNCTION_DATA_TYPE",
  222: "WASM_JS_FUNCTION_DATA_TYPE",
  223: "SMALL_ORDERED_HASH_MAP_TYPE",
  224: "SMALL_ORDERED_HASH_SET_TYPE",
  225: "SMALL_ORDERED_NAME_DICTIONARY_TYPE",
  226: "ABSTRACT_INTERNAL_CLASS_SUBCLASS1_TYPE",
  227: "ABSTRACT_INTERNAL_CLASS_SUBCLASS2_TYPE",
  228: "DESCRIPTOR_ARRAY_TYPE",
  229: "STRONG_DESCRIPTOR_ARRAY_TYPE",
  230: "SOURCE_TEXT_MODULE_TYPE",
  231: "SYNTHETIC_MODULE_TYPE",
  232: "WEAK_FIXED_ARRAY_TYPE",
  233: "TRANSITION_ARRAY_TYPE",
  234: "ACCESSOR_INFO_TYPE",
  235: "CALL_HANDLER_INFO_TYPE",
  236: "CELL_TYPE",
  237: "CODE_TYPE",
  238: "CODE_DATA_CONTAINER_TYPE",
  239: "COVERAGE_INFO_TYPE",
  240: "EMBEDDER_DATA_ARRAY_TYPE",
  241: "FEEDBACK_METADATA_TYPE",
  242: "FEEDBACK_VECTOR_TYPE",
  243: "FILLER_TYPE",
  244: "FREE_SPACE_TYPE",
  245: "INTERNAL_CLASS_TYPE",
  246: "INTERNAL_CLASS_WITH_STRUCT_ELEMENTS_TYPE",
  247: "MAP_TYPE",
  248: "MEGA_DOM_HANDLER_TYPE",
  249: "ON_HEAP_BASIC_BLOCK_PROFILER_DATA_TYPE",
  250: "PREPARSE_DATA_TYPE",
  251: "PROPERTY_ARRAY_TYPE",
  252: "PROPERTY_CELL_TYPE",
  253: "SCOPE_INFO_TYPE",
  254: "SHARED_FUNCTION_INFO_TYPE",
  255: "SMI_BOX_TYPE",
  256: "SMI_PAIR_TYPE",
  257: "SORT_STATE_TYPE",
  258: "SWISS_NAME_DICTIONARY_TYPE",
  259: "WASM_API_FUNCTION_REF_TYPE",
  260: "WASM_CONTINUATION_OBJECT_TYPE",
  261: "WASM_INTERNAL_FUNCTION_TYPE",
  262: "WASM_RESUME_DATA_TYPE",
  263: "WASM_STRING_VIEW_ITER_TYPE",
  264: "WASM_TYPE_INFO_TYPE",
  265: "WEAK_ARRAY_LIST_TYPE",
  266: "WEAK_CELL_TYPE",
  267: "WASM_ARRAY_TYPE",
  268: "WASM_STRUCT_TYPE",
  269: "JS_PROXY_TYPE",
  1057: "JS_OBJECT_TYPE",
  270: "JS_GLOBAL_OBJECT_TYPE",
  271: "JS_GLOBAL_PROXY_TYPE",
  272: "JS_MODULE_NAMESPACE_TYPE",
  1040: "JS_SPECIAL_API_OBJECT_TYPE",
  1041: "JS_PRIMITIVE_WRAPPER_TYPE",
  1058: "JS_API_OBJECT_TYPE",
  2058: "JS_LAST_DUMMY_API_OBJECT_TYPE",
  2059: "JS_DATA_VIEW_TYPE",
  2060: "JS_TYPED_ARRAY_TYPE",
  2061: "JS_ARRAY_BUFFER_TYPE",
  2062: "JS_PROMISE_TYPE",
  2063: "JS_BOUND_FUNCTION_TYPE",
  2064: "JS_WRAPPED_FUNCTION_TYPE",
  2065: "JS_FUNCTION_TYPE",
  2066: "BIGINT64_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2067: "BIGUINT64_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2068: "FLOAT32_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2069: "FLOAT64_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2070: "INT16_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2071: "INT32_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2072: "INT8_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2073: "UINT16_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2074: "UINT32_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2075: "UINT8_CLAMPED_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2076: "UINT8_TYPED_ARRAY_CONSTRUCTOR_TYPE",
  2077: "JS_ARRAY_CONSTRUCTOR_TYPE",
  2078: "JS_PROMISE_CONSTRUCTOR_TYPE",
  2079: "JS_REG_EXP_CONSTRUCTOR_TYPE",
  2080: "JS_CLASS_CONSTRUCTOR_TYPE",
  2081: "JS_ARRAY_ITERATOR_PROTOTYPE_TYPE",
  2082: "JS_ITERATOR_PROTOTYPE_TYPE",
  2083: "JS_MAP_ITERATOR_PROTOTYPE_TYPE",
  2084: "JS_OBJECT_PROTOTYPE_TYPE",
  2085: "JS_PROMISE_PROTOTYPE_TYPE",
  2086: "JS_REG_EXP_PROTOTYPE_TYPE",
  2087: "JS_SET_ITERATOR_PROTOTYPE_TYPE",
  2088: "JS_SET_PROTOTYPE_TYPE",
  2089: "JS_STRING_ITERATOR_PROTOTYPE_TYPE",
  2090: "JS_TYPED_ARRAY_PROTOTYPE_TYPE",
  2091: "JS_MAP_KEY_ITERATOR_TYPE",
  2092: "JS_MAP_KEY_VALUE_ITERATOR_TYPE",
  2093: "JS_MAP_VALUE_ITERATOR_TYPE",
  2094: "JS_SET_KEY_VALUE_ITERATOR_TYPE",
  2095: "JS_SET_VALUE_ITERATOR_TYPE",
  2096: "JS_GENERATOR_OBJECT_TYPE",
  2097: "JS_ASYNC_FUNCTION_OBJECT_TYPE",
  2098: "JS_ASYNC_GENERATOR_OBJECT_TYPE",
  2099: "JS_MAP_TYPE",
  2100: "JS_SET_TYPE",
  2101: "JS_ATOMICS_CONDITION_TYPE",
  2102: "JS_ATOMICS_MUTEX_TYPE",
  2103: "JS_WEAK_MAP_TYPE",
  2104: "JS_WEAK_SET_TYPE",
  2105: "JS_ARGUMENTS_OBJECT_TYPE",
  2106: "JS_ARRAY_TYPE",
  2107: "JS_ARRAY_ITERATOR_TYPE",
  2108: "JS_ASYNC_FROM_SYNC_ITERATOR_TYPE",
  2109: "JS_COLLATOR_TYPE",
  2110: "JS_CONTEXT_EXTENSION_OBJECT_TYPE",
  2111: "JS_DATE_TYPE",
  2112: "JS_DATE_TIME_FORMAT_TYPE",
  2113: "JS_DISPLAY_NAMES_TYPE",
  2114: "JS_DURATION_FORMAT_TYPE",
  2115: "JS_ERROR_TYPE",
  2116: "JS_EXTERNAL_OBJECT_TYPE",
  2117: "JS_FINALIZATION_REGISTRY_TYPE",
  2118: "JS_LIST_FORMAT_TYPE",
  2119: "JS_LOCALE_TYPE",
  2120: "JS_MESSAGE_OBJECT_TYPE",
  2121: "JS_NUMBER_FORMAT_TYPE",
  2122: "JS_PLURAL_RULES_TYPE",
  2123: "JS_RAW_JSON_TYPE",
  2124: "JS_REG_EXP_TYPE",
  2125: "JS_REG_EXP_STRING_ITERATOR_TYPE",
  2126: "JS_RELATIVE_TIME_FORMAT_TYPE",
  2127: "JS_SEGMENT_ITERATOR_TYPE",
  2128: "JS_SEGMENTER_TYPE",
  2129: "JS_SEGMENTS_TYPE",
  2130: "JS_SHADOW_REALM_TYPE",
  2131: "JS_SHARED_ARRAY_TYPE",
  2132: "JS_SHARED_STRUCT_TYPE",
  2133: "JS_STRING_ITERATOR_TYPE",
  2134: "JS_TEMPORAL_CALENDAR_TYPE",
  2135: "JS_TEMPORAL_DURATION_TYPE",
  2136: "JS_TEMPORAL_INSTANT_TYPE",
  2137: "JS_TEMPORAL_PLAIN_DATE_TYPE",
  2138: "JS_TEMPORAL_PLAIN_DATE_TIME_TYPE",
  2139: "JS_TEMPORAL_PLAIN_MONTH_DAY_TYPE",
  2140: "JS_TEMPORAL_PLAIN_TIME_TYPE",
  2141: "JS_TEMPORAL_PLAIN_YEAR_MONTH_TYPE",
  2142: "JS_TEMPORAL_TIME_ZONE_TYPE",
  2143: "JS_TEMPORAL_ZONED_DATE_TIME_TYPE",
  2144: "JS_V8_BREAK_ITERATOR_TYPE",
  2145: "JS_WEAK_REF_TYPE",
  2146: "WASM_EXCEPTION_PACKAGE_TYPE",
  2147: "WASM_GLOBAL_OBJECT_TYPE",
  2148: "WASM_INSTANCE_OBJECT_TYPE",
  2149: "WASM_MEMORY_OBJECT_TYPE",
  2150: "WASM_MODULE_OBJECT_TYPE",
  2151: "WASM_SUSPENDER_OBJECT_TYPE",
  2152: "WASM_TABLE_OBJECT_TYPE",
  2153: "WASM_TAG_OBJECT_TYPE",
  2154: "WASM_VALUE_OBJECT_TYPE",
}

# List of known V8 maps.
KNOWN_MAPS = {
    ("read_only_space", 0x02141): (247, "MetaMap"),
    ("read_only_space", 0x02169): (131, "NullMap"),
    ("read_only_space", 0x02191): (229, "StrongDescriptorArrayMap"),
    ("read_only_space", 0x021b9): (265, "WeakArrayListMap"),
    ("read_only_space", 0x021fd): (155, "EnumCacheMap"),
    ("read_only_space", 0x02231): (176, "FixedArrayMap"),
    ("read_only_space", 0x0227d): (8, "OneByteInternalizedStringMap"),
    ("read_only_space", 0x022c9): (244, "FreeSpaceMap"),
    ("read_only_space", 0x022f1): (243, "OnePointerFillerMap"),
    ("read_only_space", 0x02319): (243, "TwoPointerFillerMap"),
    ("read_only_space", 0x02341): (131, "UninitializedMap"),
    ("read_only_space", 0x023b9): (131, "UndefinedMap"),
    ("read_only_space", 0x023fd): (130, "HeapNumberMap"),
    ("read_only_space", 0x02431): (131, "TheHoleMap"),
    ("read_only_space", 0x02491): (131, "BooleanMap"),
    ("read_only_space", 0x02535): (191, "ByteArrayMap"),
    ("read_only_space", 0x0255d): (176, "FixedCOWArrayMap"),
    ("read_only_space", 0x02585): (177, "HashTableMap"),
    ("read_only_space", 0x025ad): (128, "SymbolMap"),
    ("read_only_space", 0x025d5): (40, "OneByteStringMap"),
    ("read_only_space", 0x025fd): (253, "ScopeInfoMap"),
    ("read_only_space", 0x02625): (254, "SharedFunctionInfoMap"),
    ("read_only_space", 0x0264d): (237, "CodeMap"),
    ("read_only_space", 0x02675): (236, "CellMap"),
    ("read_only_space", 0x0269d): (252, "GlobalPropertyCellMap"),
    ("read_only_space", 0x026c5): (204, "ForeignMap"),
    ("read_only_space", 0x026ed): (233, "TransitionArrayMap"),
    ("read_only_space", 0x02715): (45, "ThinOneByteStringMap"),
    ("read_only_space", 0x0273d): (242, "FeedbackVectorMap"),
    ("read_only_space", 0x02775): (131, "ArgumentsMarkerMap"),
    ("read_only_space", 0x027d5): (131, "ExceptionMap"),
    ("read_only_space", 0x02831): (131, "TerminationExceptionMap"),
    ("read_only_space", 0x02899): (131, "OptimizedOutMap"),
    ("read_only_space", 0x028f9): (131, "StaleRegisterMap"),
    ("read_only_space", 0x02959): (190, "ScriptContextTableMap"),
    ("read_only_space", 0x02981): (188, "ClosureFeedbackCellArrayMap"),
    ("read_only_space", 0x029a9): (241, "FeedbackMetadataArrayMap"),
    ("read_only_space", 0x029d1): (176, "ArrayListMap"),
    ("read_only_space", 0x029f9): (129, "BigIntMap"),
    ("read_only_space", 0x02a21): (189, "ObjectBoilerplateDescriptionMap"),
    ("read_only_space", 0x02a49): (192, "BytecodeArrayMap"),
    ("read_only_space", 0x02a71): (238, "CodeDataContainerMap"),
    ("read_only_space", 0x02a99): (239, "CoverageInfoMap"),
    ("read_only_space", 0x02ac1): (193, "FixedDoubleArrayMap"),
    ("read_only_space", 0x02ae9): (179, "GlobalDictionaryMap"),
    ("read_only_space", 0x02b11): (157, "ManyClosuresCellMap"),
    ("read_only_space", 0x02b39): (248, "MegaDomHandlerMap"),
    ("read_only_space", 0x02b61): (176, "ModuleInfoMap"),
    ("read_only_space", 0x02b89): (180, "NameDictionaryMap"),
    ("read_only_space", 0x02bb1): (157, "NoClosuresCellMap"),
    ("read_only_space", 0x02bd9): (182, "NumberDictionaryMap"),
    ("read_only_space", 0x02c01): (157, "OneClosureCellMap"),
    ("read_only_space", 0x02c29): (183, "OrderedHashMapMap"),
    ("read_only_space", 0x02c51): (184, "OrderedHashSetMap"),
    ("read_only_space", 0x02c79): (181, "NameToIndexHashTableMap"),
    ("read_only_space", 0x02ca1): (186, "RegisteredSymbolTableMap"),
    ("read_only_space", 0x02cc9): (185, "OrderedNameDictionaryMap"),
    ("read_only_space", 0x02cf1): (250, "PreparseDataMap"),
    ("read_only_space", 0x02d19): (251, "PropertyArrayMap"),
    ("read_only_space", 0x02d41): (234, "AccessorInfoMap"),
    ("read_only_space", 0x02d69): (235, "SideEffectCallHandlerInfoMap"),
    ("read_only_space", 0x02d91): (235, "SideEffectFreeCallHandlerInfoMap"),
    ("read_only_space", 0x02db9): (235, "NextCallSideEffectFreeCallHandlerInfoMap"),
    ("read_only_space", 0x02de1): (187, "SimpleNumberDictionaryMap"),
    ("read_only_space", 0x02e09): (223, "SmallOrderedHashMapMap"),
    ("read_only_space", 0x02e31): (224, "SmallOrderedHashSetMap"),
    ("read_only_space", 0x02e59): (225, "SmallOrderedNameDictionaryMap"),
    ("read_only_space", 0x02e81): (230, "SourceTextModuleMap"),
    ("read_only_space", 0x02ea9): (258, "SwissNameDictionaryMap"),
    ("read_only_space", 0x02ed1): (231, "SyntheticModuleMap"),
    ("read_only_space", 0x02ef9): (259, "WasmApiFunctionRefMap"),
    ("read_only_space", 0x02f21): (220, "WasmCapiFunctionDataMap"),
    ("read_only_space", 0x02f49): (221, "WasmExportedFunctionDataMap"),
    ("read_only_space", 0x02f71): (261, "WasmInternalFunctionMap"),
    ("read_only_space", 0x02f99): (222, "WasmJSFunctionDataMap"),
    ("read_only_space", 0x02fc1): (262, "WasmResumeDataMap"),
    ("read_only_space", 0x02fe9): (264, "WasmTypeInfoMap"),
    ("read_only_space", 0x03011): (260, "WasmContinuationObjectMap"),
    ("read_only_space", 0x03039): (232, "WeakFixedArrayMap"),
    ("read_only_space", 0x03061): (178, "EphemeronHashTableMap"),
    ("read_only_space", 0x03089): (240, "EmbedderDataArrayMap"),
    ("read_only_space", 0x030b1): (266, "WeakCellMap"),
    ("read_only_space", 0x030d9): (32, "StringMap"),
    ("read_only_space", 0x03101): (41, "ConsOneByteStringMap"),
    ("read_only_space", 0x03129): (33, "ConsStringMap"),
    ("read_only_space", 0x03151): (37, "ThinStringMap"),
    ("read_only_space", 0x03179): (35, "SlicedStringMap"),
    ("read_only_space", 0x031a1): (43, "SlicedOneByteStringMap"),
    ("read_only_space", 0x031c9): (34, "ExternalStringMap"),
    ("read_only_space", 0x031f1): (42, "ExternalOneByteStringMap"),
    ("read_only_space", 0x03219): (50, "UncachedExternalStringMap"),
    ("read_only_space", 0x03241): (0, "InternalizedStringMap"),
    ("read_only_space", 0x03269): (2, "ExternalInternalizedStringMap"),
    ("read_only_space", 0x03291): (10, "ExternalOneByteInternalizedStringMap"),
    ("read_only_space", 0x032b9): (18, "UncachedExternalInternalizedStringMap"),
    ("read_only_space", 0x032e1): (26, "UncachedExternalOneByteInternalizedStringMap"),
    ("read_only_space", 0x03309): (58, "UncachedExternalOneByteStringMap"),
    ("read_only_space", 0x03331): (104, "SharedOneByteStringMap"),
    ("read_only_space", 0x03359): (96, "SharedStringMap"),
    ("read_only_space", 0x03381): (106, "SharedExternalOneByteStringMap"),
    ("read_only_space", 0x033a9): (98, "SharedExternalStringMap"),
    ("read_only_space", 0x033d1): (122, "SharedUncachedExternalOneByteStringMap"),
    ("read_only_space", 0x033f9): (114, "SharedUncachedExternalStringMap"),
    ("read_only_space", 0x03421): (109, "SharedThinOneByteStringMap"),
    ("read_only_space", 0x03449): (101, "SharedThinStringMap"),
    ("read_only_space", 0x03471): (131, "SelfReferenceMarkerMap"),
    ("read_only_space", 0x03499): (131, "BasicBlockCountersMarkerMap"),
    ("read_only_space", 0x034dd): (146, "ArrayBoilerplateDescriptionMap"),
    ("read_only_space", 0x035dd): (159, "InterceptorInfoMap"),
    ("read_only_space", 0x075c9): (132, "PromiseFulfillReactionJobTaskMap"),
    ("read_only_space", 0x075f1): (133, "PromiseRejectReactionJobTaskMap"),
    ("read_only_space", 0x07619): (134, "CallableTaskMap"),
    ("read_only_space", 0x07641): (135, "CallbackTaskMap"),
    ("read_only_space", 0x07669): (136, "PromiseResolveThenableJobTaskMap"),
    ("read_only_space", 0x07691): (139, "FunctionTemplateInfoMap"),
    ("read_only_space", 0x076b9): (140, "ObjectTemplateInfoMap"),
    ("read_only_space", 0x076e1): (141, "AccessCheckInfoMap"),
    ("read_only_space", 0x07709): (142, "AccessorPairMap"),
    ("read_only_space", 0x07731): (143, "AliasedArgumentsEntryMap"),
    ("read_only_space", 0x07759): (144, "AllocationMementoMap"),
    ("read_only_space", 0x07781): (147, "AsmWasmDataMap"),
    ("read_only_space", 0x077a9): (148, "AsyncGeneratorRequestMap"),
    ("read_only_space", 0x077d1): (149, "BreakPointMap"),
    ("read_only_space", 0x077f9): (150, "BreakPointInfoMap"),
    ("read_only_space", 0x07821): (151, "CachedTemplateObjectMap"),
    ("read_only_space", 0x07849): (152, "CallSiteInfoMap"),
    ("read_only_space", 0x07871): (153, "ClassPositionsMap"),
    ("read_only_space", 0x07899): (154, "DebugInfoMap"),
    ("read_only_space", 0x078c1): (156, "ErrorStackDataMap"),
    ("read_only_space", 0x078e9): (158, "FunctionTemplateRareDataMap"),
    ("read_only_space", 0x07911): (160, "InterpreterDataMap"),
    ("read_only_space", 0x07939): (161, "ModuleRequestMap"),
    ("read_only_space", 0x07961): (162, "PromiseCapabilityMap"),
    ("read_only_space", 0x07989): (163, "PromiseOnStackMap"),
    ("read_only_space", 0x079b1): (164, "PromiseReactionMap"),
    ("read_only_space", 0x079d9): (165, "PropertyDescriptorObjectMap"),
    ("read_only_space", 0x07a01): (166, "PrototypeInfoMap"),
    ("read_only_space", 0x07a29): (167, "RegExpBoilerplateDescriptionMap"),
    ("read_only_space", 0x07a51): (168, "ScriptMap"),
    ("read_only_space", 0x07a79): (169, "ScriptOrModuleMap"),
    ("read_only_space", 0x07aa1): (170, "SourceTextModuleInfoEntryMap"),
    ("read_only_space", 0x07ac9): (171, "StackFrameInfoMap"),
    ("read_only_space", 0x07af1): (172, "TemplateObjectDescriptionMap"),
    ("read_only_space", 0x07b19): (173, "Tuple2Map"),
    ("read_only_space", 0x07b41): (174, "WasmExceptionTagMap"),
    ("read_only_space", 0x07b69): (175, "WasmIndirectFunctionTableMap"),
    ("read_only_space", 0x07b91): (195, "SloppyArgumentsElementsMap"),
    ("read_only_space", 0x07bb9): (228, "DescriptorArrayMap"),
    ("read_only_space", 0x07be1): (217, "UncompiledDataWithoutPreparseDataMap"),
    ("read_only_space", 0x07c09): (215, "UncompiledDataWithPreparseDataMap"),
    ("read_only_space", 0x07c31): (218, "UncompiledDataWithoutPreparseDataWithJobMap"),
    ("read_only_space", 0x07c59): (216, "UncompiledDataWithPreparseDataAndJobMap"),
    ("read_only_space", 0x07c81): (249, "OnHeapBasicBlockProfilerDataMap"),
    ("read_only_space", 0x07ca9): (196, "TurbofanBitsetTypeMap"),
    ("read_only_space", 0x07cd1): (200, "TurbofanUnionTypeMap"),
    ("read_only_space", 0x07cf9): (199, "TurbofanRangeTypeMap"),
    ("read_only_space", 0x07d21): (197, "TurbofanHeapConstantTypeMap"),
    ("read_only_space", 0x07d49): (198, "TurbofanOtherNumberConstantTypeMap"),
    ("read_only_space", 0x07d71): (245, "InternalClassMap"),
    ("read_only_space", 0x07d99): (256, "SmiPairMap"),
    ("read_only_space", 0x07dc1): (255, "SmiBoxMap"),
    ("read_only_space", 0x07de9): (201, "ExportedSubClassBaseMap"),
    ("read_only_space", 0x07e11): (202, "ExportedSubClassMap"),
    ("read_only_space", 0x07e39): (226, "AbstractInternalClassSubclass1Map"),
    ("read_only_space", 0x07e61): (227, "AbstractInternalClassSubclass2Map"),
    ("read_only_space", 0x07e89): (194, "InternalClassWithSmiElementsMap"),
    ("read_only_space", 0x07eb1): (246, "InternalClassWithStructElementsMap"),
    ("read_only_space", 0x07ed9): (203, "ExportedSubClass2Map"),
    ("read_only_space", 0x07f01): (257, "SortStateMap"),
    ("read_only_space", 0x07f29): (263, "WasmStringViewIterMap"),
    ("read_only_space", 0x07f51): (145, "AllocationSiteWithWeakNextMap"),
    ("read_only_space", 0x07f79): (145, "AllocationSiteWithoutWeakNextMap"),
    ("read_only_space", 0x08045): (137, "LoadHandler1Map"),
    ("read_only_space", 0x0806d): (137, "LoadHandler2Map"),
    ("read_only_space", 0x08095): (137, "LoadHandler3Map"),
    ("read_only_space", 0x080bd): (138, "StoreHandler0Map"),
    ("read_only_space", 0x080e5): (138, "StoreHandler1Map"),
    ("read_only_space", 0x0810d): (138, "StoreHandler2Map"),
    ("read_only_space", 0x08135): (138, "StoreHandler3Map"),
    ("old_space", 0x043a5): (2116, "ExternalMap"),
    ("old_space", 0x043d5): (2120, "JSMessageObjectMap"),
}

# List of known V8 objects.
KNOWN_OBJECTS = {
  ("read_only_space", 0x021e1): "EmptyWeakArrayList",
  ("read_only_space", 0x021ed): "EmptyDescriptorArray",
  ("read_only_space", 0x02225): "EmptyEnumCache",
  ("read_only_space", 0x02259): "EmptyFixedArray",
  ("read_only_space", 0x02261): "NullValue",
  ("read_only_space", 0x02369): "UninitializedValue",
  ("read_only_space", 0x023e1): "UndefinedValue",
  ("read_only_space", 0x02425): "NanValue",
  ("read_only_space", 0x02459): "TheHoleValue",
  ("read_only_space", 0x02485): "HoleNanValue",
  ("read_only_space", 0x024b9): "TrueValue",
  ("read_only_space", 0x024f9): "FalseValue",
  ("read_only_space", 0x02529): "empty_string",
  ("read_only_space", 0x02765): "EmptyScopeInfo",
  ("read_only_space", 0x0279d): "ArgumentsMarker",
  ("read_only_space", 0x027fd): "Exception",
  ("read_only_space", 0x02859): "TerminationException",
  ("read_only_space", 0x028c1): "OptimizedOut",
  ("read_only_space", 0x02921): "StaleRegister",
  ("read_only_space", 0x034c1): "EmptyPropertyArray",
  ("read_only_space", 0x034c9): "EmptyByteArray",
  ("read_only_space", 0x034d1): "EmptyObjectBoilerplateDescription",
  ("read_only_space", 0x03505): "EmptyArrayBoilerplateDescription",
  ("read_only_space", 0x03511): "EmptyClosureFeedbackCellArray",
  ("read_only_space", 0x03519): "EmptySlowElementDictionary",
  ("read_only_space", 0x0353d): "EmptyOrderedHashMap",
  ("read_only_space", 0x03551): "EmptyOrderedHashSet",
  ("read_only_space", 0x03565): "EmptyFeedbackMetadata",
  ("read_only_space", 0x03571): "EmptyPropertyDictionary",
  ("read_only_space", 0x03599): "EmptyOrderedPropertyDictionary",
  ("read_only_space", 0x035b1): "EmptySwissPropertyDictionary",
  ("read_only_space", 0x03605): "NoOpInterceptorInfo",
  ("read_only_space", 0x0362d): "EmptyArrayList",
  ("read_only_space", 0x03639): "EmptyWeakFixedArray",
  ("read_only_space", 0x03641): "InfinityValue",
  ("read_only_space", 0x0364d): "MinusZeroValue",
  ("read_only_space", 0x03659): "MinusInfinityValue",
  ("read_only_space", 0x03665): "SingleCharacterStringTable",
  ("read_only_space", 0x04a6d): "SelfReferenceMarker",
  ("read_only_space", 0x04aad): "BasicBlockCountersMarker",
  ("read_only_space", 0x04af1): "OffHeapTrampolineRelocationInfo",
  ("read_only_space", 0x04afd): "GlobalThisBindingScopeInfo",
  ("read_only_space", 0x04b2d): "EmptyFunctionScopeInfo",
  ("read_only_space", 0x04b51): "NativeScopeInfo",
  ("read_only_space", 0x04b69): "HashSeed",
  ("old_space", 0x0423d): "ArgumentsIteratorAccessor",
  ("old_space", 0x04255): "ArrayLengthAccessor",
  ("old_space", 0x0426d): "BoundFunctionLengthAccessor",
  ("old_space", 0x04285): "BoundFunctionNameAccessor",
  ("old_space", 0x0429d): "ErrorStackAccessor",
  ("old_space", 0x042b5): "FunctionArgumentsAccessor",
  ("old_space", 0x042cd): "FunctionCallerAccessor",
  ("old_space", 0x042e5): "FunctionNameAccessor",
  ("old_space", 0x042fd): "FunctionLengthAccessor",
  ("old_space", 0x04315): "FunctionPrototypeAccessor",
  ("old_space", 0x0432d): "SharedArrayLengthAccessor",
  ("old_space", 0x04345): "StringLengthAccessor",
  ("old_space", 0x0435d): "ValueUnavailableAccessor",
  ("old_space", 0x04375): "WrappedFunctionLengthAccessor",
  ("old_space", 0x0438d): "WrappedFunctionNameAccessor",
  ("old_space", 0x043a5): "ExternalMap",
  ("old_space", 0x043cd): "InvalidPrototypeValidityCell",
  ("old_space", 0x043d5): "JSMessageObjectMap",
  ("old_space", 0x043fd): "EmptyScript",
  ("old_space", 0x04441): "ManyClosuresCell",
  ("old_space", 0x0444d): "ArrayConstructorProtector",
  ("old_space", 0x04461): "NoElementsProtector",
  ("old_space", 0x04475): "MegaDOMProtector",
  ("old_space", 0x04489): "IsConcatSpreadableProtector",
  ("old_space", 0x0449d): "ArraySpeciesProtector",
  ("old_space", 0x044b1): "TypedArraySpeciesProtector",
  ("old_space", 0x044c5): "PromiseSpeciesProtector",
  ("old_space", 0x044d9): "RegExpSpeciesProtector",
  ("old_space", 0x044ed): "StringLengthProtector",
  ("old_space", 0x04501): "ArrayIteratorProtector",
  ("old_space", 0x04515): "ArrayBufferDetachingProtector",
  ("old_space", 0x04529): "PromiseHookProtector",
  ("old_space", 0x0453d): "PromiseResolveProtector",
  ("old_space", 0x04551): "MapIteratorProtector",
  ("old_space", 0x04565): "PromiseThenProtector",
  ("old_space", 0x04579): "SetIteratorProtector",
  ("old_space", 0x0458d): "StringIteratorProtector",
  ("old_space", 0x045a1): "StringSplitCache",
  ("old_space", 0x049a9): "RegExpMultipleCache",
  ("old_space", 0x04db1): "BuiltinsConstantsTable",
  ("old_space", 0x0520d): "AsyncFunctionAwaitRejectSharedFun",
  ("old_space", 0x05231): "AsyncFunctionAwaitResolveSharedFun",
  ("old_space", 0x05255): "AsyncGeneratorAwaitRejectSharedFun",
  ("old_space", 0x05279): "AsyncGeneratorAwaitResolveSharedFun",
  ("old_space", 0x0529d): "AsyncGeneratorYieldWithAwaitResolveSharedFun",
  ("old_space", 0x052c1): "AsyncGeneratorReturnResolveSharedFun",
  ("old_space", 0x052e5): "AsyncGeneratorReturnClosedRejectSharedFun",
  ("old_space", 0x05309): "AsyncGeneratorReturnClosedResolveSharedFun",
  ("old_space", 0x0532d): "AsyncIteratorValueUnwrapSharedFun",
  ("old_space", 0x05351): "PromiseAllResolveElementSharedFun",
  ("old_space", 0x05375): "PromiseAllSettledResolveElementSharedFun",
  ("old_space", 0x05399): "PromiseAllSettledRejectElementSharedFun",
  ("old_space", 0x053bd): "PromiseAnyRejectElementSharedFun",
  ("old_space", 0x053e1): "PromiseCapabilityDefaultRejectSharedFun",
  ("old_space", 0x05405): "PromiseCapabilityDefaultResolveSharedFun",
  ("old_space", 0x05429): "PromiseCatchFinallySharedFun",
  ("old_space", 0x0544d): "PromiseGetCapabilitiesExecutorSharedFun",
  ("old_space", 0x05471): "PromiseThenFinallySharedFun",
  ("old_space", 0x05495): "PromiseThrowerFinallySharedFun",
  ("old_space", 0x054b9): "PromiseValueThunkFinallySharedFun",
  ("old_space", 0x054dd): "ProxyRevokeSharedFun",
  ("old_space", 0x05501): "ShadowRealmImportValueFulfilledSFI",
  ("old_space", 0x05525): "SourceTextModuleExecuteAsyncModuleFulfilledSFI",
  ("old_space", 0x05549): "SourceTextModuleExecuteAsyncModuleRejectedSFI",
}

# Lower 32 bits of first page addresses for various heap spaces.
HEAP_FIRST_PAGES = {
  0x000c0000: "old_space",
  0x00000000: "read_only_space",
}

# List of known V8 Frame Markers.
FRAME_MARKERS = (
  "ENTRY",
  "CONSTRUCT_ENTRY",
  "EXIT",
  "WASM",
  "WASM_TO_JS",
  "WASM_TO_JS_FUNCTION",
  "JS_TO_WASM",
  "STACK_SWITCH",
  "WASM_DEBUG_BREAK",
  "C_WASM_ENTRY",
  "WASM_EXIT",
  "WASM_COMPILE_LAZY",
  "INTERPRETED",
  "BASELINE",
  "MAGLEV",
  "TURBOFAN",
  "STUB",
  "TURBOFAN_STUB_WITH_CONTEXT",
  "BUILTIN_CONTINUATION",
  "JAVA_SCRIPT_BUILTIN_CONTINUATION",
  "JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH",
  "INTERNAL",
  "CONSTRUCT",
  "BUILTIN",
  "BUILTIN_EXIT",
  "NATIVE",
)

# This set of constants is generated from a shipping build.
