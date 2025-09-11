# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2023 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

include(blst)

add_library(evmone_precompiles STATIC)
add_library(evmone::precompiles ALIAS evmone_precompiles)
target_link_libraries(evmone_precompiles PUBLIC evmc::evmc_cpp PRIVATE evmone::evmmax blst::blst)
target_include_directories(evmone_precompiles INTERFACE ..)
target_sources(
    evmone_precompiles PRIVATE
    blake2b.hpp
    blake2b.cpp
    bls.hpp
    bls.cpp
    bn254.hpp
    bn254.cpp
    ecc.hpp
    hash_types.h
    keccak.h
    keccak.hpp
    keccak.c
    pairing/bn254/fields.hpp
    pairing/bn254/pairing.cpp
    pairing/bn254/utils.hpp
    pairing/field_template.hpp
    modexp.hpp
    modexp.cpp
    ripemd160.hpp
    ripemd160.cpp
    secp256k1.hpp
    secp256k1.cpp
    secp256r1.hpp
    secp256r1.cpp
    sha256.hpp
    sha256.cpp
    kzg.hpp
    kzg.cpp
)
