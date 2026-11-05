# evmone-fuzzer: LibFuzzer based testing tool for EVMC-compatible EVM implementations.
# Copyright 2019 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0
include(ExternalProject)

get_target_property(type evmone TYPE)
if(NOT type STREQUAL STATIC_LIBRARY)
    message(FATAL_ERROR "The evmone must be built as static library")
endif()

if(COVERAGE)
    set(CMAKE_EXE_LINKER_FLAGS "-fsanitize=fuzzer")
else()
    string(REPLACE fuzzer-no-link fuzzer CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS})
endif()

add_executable(evmone-fuzzer fuzzer.cpp)
target_link_libraries(evmone-fuzzer PRIVATE evmone evmone::testutils)
