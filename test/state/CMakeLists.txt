# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2022 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

include(FetchContent)

add_library(evmone-state STATIC)
add_library(evmone::state ALIAS evmone-state)
target_compile_features(evmone-state PUBLIC cxx_std_20)
target_link_libraries(evmone-state PUBLIC evmone::precompiles evmc::evmc_cpp PRIVATE evmone)
target_include_directories(evmone-state PUBLIC ${PROJECT_SOURCE_DIR})
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
    precompiles.hpp
    precompiles.cpp
    precompiles_internal.hpp
    requests.hpp
    requests.cpp
    state.hpp
    state.cpp
    state_diff.hpp
    state_view.hpp
    system_contracts.hpp
    system_contracts.cpp
    transaction.hpp
)

option(EVMONE_PRECOMPILES_LIBSECP256K1 "Enable precompiles implementations using libsecp256k1 library" OFF)
if(EVMONE_PRECOMPILES_LIBSECP256K1)
    FetchContent_Declare(
        libsecp256k1
        URL https://github.com/bitcoin-core/secp256k1/archive/refs/tags/v0.7.1.tar.gz
        URL_HASH SHA256=958f204dbafc117e73a2604285dc2eb2a5128344d3499c114dcba5de54cb7a9e
    )
    # TODO(cmake-3.25): Use block().
    set(BUILD_SHARED_LIBS_ORIG ${BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS OFF)
    set(SECP256K1_ENABLE_MODULE_ECDH OFF)
    set(SECP256K1_ENABLE_MODULE_RECOVERY ON)
    set(SECP256K1_ENABLE_MODULE_EXTRAKEYS OFF)
    set(SECP256K1_ENABLE_MODULE_SCHNORRSIG OFF)
    set(SECP256K1_ENABLE_MODULE_MUSIG OFF)
    set(SECP256K1_ENABLE_MODULE_ELLSWIFT OFF)
    # On x86_64 using asm is ~1% worse. Untested on ARM.
    set(SECP256K1_ASM OFF)
    set(SECP256K1_BUILD_BENCHMARK OFF)
    set(SECP256K1_BUILD_TESTS OFF)
    set(SECP256K1_BUILD_EXHAUSTIVE_TESTS OFF)
    set(SECP256K1_BUILD_CTIME_TESTS OFF)
    set(SECP256K1_BUILD_EXAMPLES OFF)
    # Not needed for static library.
    set(SECP256K1_ENABLE_API_VISIBILITY_ATTRIBUTES OFF)
    set(SECP256K1_VALGRIND OFF)
    FetchContent_MakeAvailable(libsecp256k1)
    foreach(TARGET secp256k1 secp256k1_precomputed)
        set_target_properties(
            ${TARGET}
            PROPERTIES
            COMPILE_OPTIONS $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-w>  # Disable warnings.
            CXX_CLANG_TIDY ""
        )
    endforeach()
    add_library(libsecp256k1::secp256k1 ALIAS secp256k1)

    target_link_libraries(evmone-state PRIVATE libsecp256k1::secp256k1)
    target_compile_definitions(evmone-state PUBLIC EVMONE_PRECOMPILES_LIBSECP256K1=1)
    target_sources(
        evmone-state PRIVATE
        precompiles_libsecp256k1.hpp
        precompiles_libsecp256k1.cpp
    )
    set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_ORIG})
endif()

option(EVMONE_PRECOMPILES_SILKPRE "Enable precompiles from silkpre library (for benchmarking only)" OFF)
if(EVMONE_PRECOMPILES_SILKPRE)
    # Silkpre requires the ethash library, so enable it in Hunter.
    hunter_add_package(ethash)

    FetchContent_Declare(
        silkpre
        GIT_REPOSITORY https://github.com/erigontech/silkpre
        GIT_TAG f972090a41de75073f55e914bf0107838b4b86a4
        GIT_SHALLOW TRUE
    )
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
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
