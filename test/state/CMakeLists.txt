# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2022 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

add_library(evmone-state STATIC)
add_library(evmone::state ALIAS evmone-state)
target_link_libraries(evmone-state PUBLIC evmone::precompiles evmc::evmc_cpp PRIVATE evmone)
target_include_directories(evmone-state PRIVATE ${evmone_private_include_dir})
target_sources(
    evmone-state PRIVATE
    account.hpp
    blob_params.hpp
    block.hpp
    block.cpp
    bloom_filter.hpp
    bloom_filter.cpp
    errors.hpp
    ethash_difficulty.hpp
    ethash_difficulty.cpp
    hash_utils.hpp
    host.hpp
    host.cpp
    mpt.hpp
    mpt.cpp
    mpt_hash.hpp
    mpt_hash.cpp
    precompiles.hpp
    precompiles.cpp
    precompiles_internal.hpp
    precompiles_stubs.hpp
    precompiles_stubs.cpp
    requests.hpp
    requests.cpp
    rlp.hpp
    state.hpp
    state.cpp
    state_diff.hpp
    state_view.hpp
    system_contracts.hpp
    system_contracts.cpp
    test_state.hpp
    test_state.cpp
    transaction.hpp
    transaction.cpp
)

option(EVMONE_PRECOMPILES_SILKPRE "Enable precompiles from silkpre library (for benchmarking only)" OFF)
if(EVMONE_PRECOMPILES_SILKPRE)
    # Silkpre requires the ethash library, so enable it in Hunter.
    hunter_add_package(ethash)

    include(FetchContent)
    FetchContent_Declare(
        silkpre
        GIT_REPOSITORY https://github.com/erigontech/silkpre
        GIT_TAG f972090a41de75073f55e914bf0107838b4b86a4
        GIT_SHALLOW TRUE
    )
    set(BUILD_SHARED_LIBS_ORIG ${BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS OFF)
    FetchContent_MakeAvailable(silkpre)
    set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_ORIG})

    set_target_properties(
        silkpre secp256k1 ff
        PROPERTIES
        COMPILE_OPTIONS -w  # Disable warnings.
        CXX_CLANG_TIDY ""
    )

    target_link_libraries(evmone-state PRIVATE silkpre)
    target_compile_definitions(evmone-state PUBLIC EVMONE_PRECOMPILES_SILKPRE=1)
    target_sources(
        evmone-state PRIVATE
        precompiles_silkpre.hpp
        precompiles_silkpre.cpp
    )
endif()

# This is done after Silkpre because it also tries to find GMP.
option(EVMONE_PRECOMPILES_GMP "Enable precompiles implementations via the GMP/MPIR library" OFF)
if(EVMONE_PRECOMPILES_GMP)
    find_package(GMP REQUIRED)
    target_link_libraries(evmone-state PRIVATE GMP::gmp)
    target_compile_definitions(evmone-state PUBLIC EVMONE_PRECOMPILES_GMP=1)
    target_sources(
        evmone-state PRIVATE
        precompiles_gmp.hpp
        precompiles_gmp.cpp
    )
endif()
