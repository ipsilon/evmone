# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2019 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

add_library(evmone.testutils STATIC)
add_library(evmone::testutils ALIAS evmone.testutils)
target_link_libraries(
    evmone.testutils
    PUBLIC evmone::state evmc::evmc_cpp nlohmann_json::nlohmann_json
    PRIVATE evmc::mocked_host
)

target_sources(
    evmone.testutils
    PRIVATE
    stdx/utility.hpp
    benchmark_recorder.hpp
    benchmark_recorder.cpp
    blob_schedule.hpp
    blob_schedule.cpp
    block_transition.hpp
    block_transition.cpp
    blockchaintest.hpp
    blockchaintest_loader.cpp
    blockchaintest_runner.cpp
    bytecode.hpp
    error_matching.hpp
    error_matching.cpp
    mpt.hpp
    mpt.cpp
    mpt_hash.hpp
    mpt_hash.cpp
    rlp.hpp
    rlp_encode.hpp
    rlp_encode.cpp
    run.hpp
    run.cpp
    statetest.hpp
    statetest_export.cpp
    statetest_loader.cpp
    statetest_logs_hash.cpp
    statetest_runner.cpp
    t8n.hpp
    t8n.cpp
    test_collector.hpp
    test_collector.cpp
    test_driver.hpp
    test_driver.cpp
    test_report.hpp
    test_report.cpp
    test_state.hpp
    test_state.cpp
    utils.hpp
    utils.cpp
)

# The hooks report each test to the CodSpeed runner. They come with the fork of
# google/benchmark, which the parent directory fetches, and are used without it: the target is
# defined after this one, which linking tolerates, but only when that directory is configured.
if(CODSPEED_MODE AND EVMONE_TESTING)
    target_link_libraries(evmone.testutils PRIVATE codspeed instrument_hooks)
    set_source_files_properties(
        benchmark_recorder.cpp PROPERTIES
        COMPILE_DEFINITIONS "EVMONE_CODSPEED;PROJECT_VERSION=\"${PROJECT_VERSION}\""
    )
endif()
