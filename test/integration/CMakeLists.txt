# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2021 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

set(PREFIX ${PROJECT_NAME}/integration)

if(TARGET evmone-cli)
    add_subdirectory(evmone-cli)
endif()

add_subdirectory(export)
