# EVMC: Ethereum Client-VM Connector API.
# Copyright 2021 The EVMC Authors.
# Licensed under the Apache License, Version 2.0.

add_library(tooling STATIC)
add_library(evmc::tooling ALIAS tooling)
target_compile_features(tooling PUBLIC cxx_std_17)
target_link_libraries(tooling PUBLIC evmc::evmc_cpp evmc::mocked_host)

target_sources(
    tooling PRIVATE
    ${EVMC_INCLUDE_DIR}/evmc/tooling.hpp
    run.cpp
)
