# EVMC: Ethereum Client-VM Connector API.
# Copyright 2018 The EVMC Authors.
# Licensed under the Apache License, Version 2.0.

add_library(
    loader STATIC
    ${EVMC_INCLUDE_DIR}/evmc/loader.h
    loader.c
)

add_library(evmc::loader ALIAS loader)
set_target_properties(loader PROPERTIES
    OUTPUT_NAME evmc-loader
    POSITION_INDEPENDENT_CODE TRUE
)
target_link_libraries(loader INTERFACE ${CMAKE_DL_LIBS} PUBLIC evmc::evmc)
