# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2022 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

add_library(evmone-statetestutils STATIC)
add_library(evmone::statetestutils ALIAS evmone-statetestutils)
target_compile_features(evmone-statetestutils PUBLIC cxx_std_20)
target_link_libraries(evmone-statetestutils PRIVATE evmone::state evmone::testutils nlohmann_json::nlohmann_json GTest::gtest)
target_include_directories(evmone-statetestutils PRIVATE ${evmone_private_include_dir})
target_sources(
    evmone-statetestutils PRIVATE
    ../blockchaintest/blockchaintest_loader.cpp
    statetest.hpp
    statetest_export.cpp
    statetest_loader.cpp
    statetest_logs_hash.cpp
    statetest_runner.cpp
)

add_executable(evmone-statetest)
target_link_libraries(evmone-statetest PRIVATE evmone::statetestutils evmone evmone-buildinfo GTest::gtest)
target_include_directories(evmone-statetest PRIVATE ${evmone_private_include_dir})
target_sources(
    evmone-statetest PRIVATE
    statetest.cpp
)
