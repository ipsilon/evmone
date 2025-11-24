# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2019 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

find_package(intx CONFIG REQUIRED)

add_library(evmone.testutils STATIC)
add_library(evmone::testutils ALIAS evmone.testutils)
target_link_libraries(evmone.testutils PUBLIC evmc::evmc_cpp)
target_include_directories(evmone.testutils INTERFACE ${PROJECT_SOURCE_DIR} ${evmone_private_include_dir})

target_sources(
    evmone.testutils
    PRIVATE
    blob_schedule.hpp
    blob_schedule.cpp
    bytecode.hpp
    stdx/utility.hpp
    utils.hpp
    utils.cpp
)
