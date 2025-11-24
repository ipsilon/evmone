# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2022 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

add_executable(evmone-statetest)
target_link_libraries(evmone-statetest PRIVATE evmone::testutils evmone evmone-buildinfo GTest::gtest)
target_sources(
    evmone-statetest PRIVATE
    statetest.cpp
    statetest_runner.cpp
)
