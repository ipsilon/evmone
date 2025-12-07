# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2023 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

add_executable(evmone-blockchaintest)
target_link_libraries(evmone-blockchaintest PRIVATE evmone::testutils evmone evmone-buildinfo GTest::gtest)
target_sources(
    evmone-blockchaintest PRIVATE
    blockchaintest.cpp
    blockchaintest_runner.hpp
    blockchaintest_runner.cpp
)
