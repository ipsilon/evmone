# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2018 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

include(hunter_cmake_args)

hunter_config(
    intx
    VERSION 0.14.0
    URL https://github.com/chfast/intx/archive/v0.14.0.tar.gz
    SHA1 f52681180c5fcd75a6b257923a55ccba6b084adf
)

hunter_config(
    GTest
    VERSION 1.17.0
    URL https://github.com/google/googletest/archive/v1.17.0.tar.gz
    SHA1 8a8ee424e8275ec4b480ba4a0d1ba94b5dee3ee4
)

hunter_config(
    CLI11
    VERSION 2.5.0
    URL https://github.com/CLIUtils/CLI11/archive/v2.5.0.tar.gz
    SHA1 8411927bd2fa7c8fc6dff4c53a31cde4a9017f9c
)

hunter_config(
    nlohmann_json
    VERSION 3.12.0
    URL https://github.com/nlohmann/json/archive/v3.12.0.tar.gz
    SHA1 815212d8acbddc87009667c52ba98a8404efec18
)

# Propagate BENCHMARK_ENABLE_LIBPFM to google/benchmark.
# https://github.com/google/benchmark/blob/v1.9.4/CMakeLists.txt#L42
option(BENCHMARK_ENABLE_LIBPFM "Enable performance counters provided by libpfm" OFF)

hunter_config(
    benchmark
    VERSION 1.9.4
    CMAKE_ARGS BENCHMARK_ENABLE_LIBPFM=${BENCHMARK_ENABLE_LIBPFM}
    URL https://github.com/google/benchmark/archive/v1.9.4.tar.gz
    SHA1 46984dfbfc5fbfa42a0b60bfd3a962ef0d7d1c93
)
