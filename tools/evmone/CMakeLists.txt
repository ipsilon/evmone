# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2025 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

hunter_add_package(CLI11)
find_package(CLI11 CONFIG REQUIRED)

add_executable(evmone-cli main.cpp)
set_target_properties(evmone-cli PROPERTIES OUTPUT_NAME evmone)
target_link_libraries(evmone-cli PRIVATE evmone evmc::tooling CLI11::CLI11)
