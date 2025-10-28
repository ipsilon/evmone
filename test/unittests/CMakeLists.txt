# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2018-2020 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

# The internal evmone unit tests. The generic EVM ones are also built in.
add_executable(evmone-unittests)
target_sources(
    evmone-unittests PRIVATE
    analysis_test.cpp
    baseline_analysis_test.cpp
    blockchaintest_loader_test.cpp
    bytecode_test.cpp
    evm_fixture.cpp
    evm_fixture.hpp
    evm_test.cpp
    evm_calls_test.cpp
    evm_control_flow_test.cpp
    evm_eip2929_test.cpp
    evm_eip3198_basefee_test.cpp
    evm_eip3855_push0_test.cpp
    evm_eip3860_initcode_test.cpp
    evm_eip4844_blobhash_test.cpp
    evm_eip7516_blobbasefee_test.cpp
    evm_eip7939_clz_test.cpp
    evm_memory_test.cpp
    evm_state_test.cpp
    evm_storage_test.cpp
    evm_other_test.cpp
    evm_benchmark_test.cpp
    evm_undefined_instructions_test.cpp
    evmmax_bn254_add_test.cpp
    evmmax_bn254_mul_test.cpp
    evmmax_bn254_pairing_test.cpp
    evmmax_test.cpp
    evmmax_secp256k1_test.cpp
    evmone_test.cpp
    execution_state_test.cpp
    exportable_fixture.hpp
    exportable_fixture.cpp
    instructions_test.cpp
    jumpdest_analysis_test.cpp
    lru_cache_test.cpp
    precompiles_blake2b_test.cpp
    precompiles_bls_test.cpp
    precompiles_bn254_test.cpp
    precompiles_kzg_test.cpp
    precompiles_ripemd160_test.cpp
    precompiles_secp256r1_test.cpp
    precompiles_sha256_test.cpp
    precompiles_expmod_test.cpp
    state_block_test.cpp
    state_bloom_filter_test.cpp
    state_deposit_requests_test.cpp
    state_difficulty_test.cpp
    state_mpt_hash_test.cpp
    state_mpt_test.cpp
    state_new_account_address_test.cpp
    state_precompiles_test.cpp
    state_rlp_test.cpp
    state_system_call_test.cpp
    state_transition.hpp
    state_transition.cpp
    state_transition_block_test.cpp
    state_transition_call_test.cpp
    state_transition_create_test.cpp
    state_transition_eip7702_test.cpp
    state_transition_extcode_test.cpp
    state_transition_selfdestruct_test.cpp
    state_transition_snippets_test.cpp
    state_transition_touch_test.cpp
    state_transition_trace_test.cpp
    state_transition_transient_storage_test.cpp
    state_transition_tx_test.cpp
    state_tx_test.cpp
    statetest_loader_block_info_test.cpp
    statetest_loader_test.cpp
    statetest_loader_tx_test.cpp
    statetest_logs_hash_test.cpp
    statetest_withdrawals_test.cpp
    tracing_test.cpp
)
target_link_libraries(evmone-unittests PRIVATE evmone evmone::evmmax evmone::experimental evmone::state evmone::statetestutils testutils evmc::instructions GTest::gtest GTest::gtest_main)
target_include_directories(evmone-unittests PRIVATE ${evmone_private_include_dir})
target_compile_options(evmone-unittests PRIVATE
    # Disable false positive C4789
    # https://developercommunity.visualstudio.com/t/False-positive-buffer-overrun-warning-C/10666762
    $<$<CXX_COMPILER_ID:MSVC>:-wd4789>
)

gtest_discover_tests(
    evmone-unittests
    TEST_PREFIX ${PROJECT_NAME}/unittests/
    PROPERTIES
    ENVIRONMENT LLVM_PROFILE_FILE=${PROJECT_BINARY_DIR}/unittests-%p.profraw
)

option(EVMONE_EVM_TEST_TOOL "Enable EVM unit testing tool for EVMC implementations (not maintained)" OFF)
if(EVMONE_EVM_TEST_TOOL)
    # The evm-test tool that contains the all evm-unittests and loads VMs as EVMC modules.
    add_executable(evm-test main.cpp)
    target_link_libraries(evm-test PRIVATE evm-unittests testutils evmc::evmc evmc::loader GTest::gtest)
endif()

# Provide the project version to selected source files.
set_source_files_properties(
    evmone_test.cpp
    main.cpp
    PROPERTIES COMPILE_DEFINITIONS PROJECT_VERSION="${PROJECT_VERSION}"
)
