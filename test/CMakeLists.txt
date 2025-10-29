# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2019-2020 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

include(GoogleTest)
include(${PROJECT_SOURCE_DIR}/evmc/cmake/EVMC.cmake)

set(evmone_private_include_dir ${PROJECT_SOURCE_DIR}/lib)

hunter_add_package(GTest)
find_package(GTest CONFIG REQUIRED)

hunter_add_package(benchmark)
find_package(benchmark CONFIG REQUIRED)

hunter_add_package(nlohmann_json)
find_package(nlohmann_json CONFIG REQUIRED)

add_subdirectory(utils)
add_subdirectory(bench)
add_subdirectory(blockchaintest)
add_subdirectory(experimental)
add_subdirectory(integration)
add_subdirectory(internal_benchmarks)
add_subdirectory(precompiles_bench)
add_subdirectory(state)
add_subdirectory(statetest)
add_subdirectory(t8n)
add_subdirectory(unittests)

set(targets evmone-bench evmone-bench-internal evmone-blockchaintest evmone-precompiles-bench evmone-state evmone-statetest evmone-t8n evmone-unittests)

if(EVMONE_FUZZING)
    add_subdirectory(fuzzer)
    list(APPEND targets evmone-fuzzer)
endif()

set_target_properties(
    ${targets} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}
)

get_target_property(type evmone TYPE)
if(type STREQUAL SHARED_LIBRARY)
    evmc_add_vm_test(NAME ${PROJECT_NAME}/evmc-vmtester TARGET evmone)

    if(TARGET evm-test)
        add_test(NAME ${PROJECT_NAME}/evm-test COMMAND evm-test $<TARGET_FILE:evmone>)
    endif()
endif()
