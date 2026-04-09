# EVMC: Ethereum Client-VM Connector API.
# Copyright 2018 The EVMC Authors.
# Licensed under the Apache License, Version 2.0.

add_library(evmc INTERFACE)
add_library(evmc::evmc ALIAS evmc)
target_compile_features(evmc INTERFACE c_std_99)
target_include_directories(evmc INTERFACE $<BUILD_INTERFACE:${EVMC_INCLUDE_DIR}>$<INSTALL_INTERFACE:include>)

add_library(evmc_cpp INTERFACE)
add_library(evmc::evmc_cpp ALIAS evmc_cpp)
target_compile_features(evmc_cpp INTERFACE cxx_std_17)
target_include_directories(evmc_cpp INTERFACE $<BUILD_INTERFACE:${EVMC_INCLUDE_DIR}>$<INSTALL_INTERFACE:include>)
target_link_libraries(evmc_cpp INTERFACE evmc::evmc)

add_subdirectory(mocked_host)
add_subdirectory(tooling)
