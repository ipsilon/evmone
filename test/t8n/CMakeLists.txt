# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2022 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

add_executable(evmone-t8n)
target_link_libraries(evmone-t8n PRIVATE evmone::testutils evmone-buildinfo intx::intx)
target_sources(evmone-t8n PRIVATE t8n.cpp)
