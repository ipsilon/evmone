# EVMC: Ethereum Client-VM Connector API.
# Copyright 2016 The EVMC Authors.
# Licensed under the Apache License, Version 2.0.

if(TARGET evmc)
    # The evmc library has been already created (probably by other submodule).
    return()
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cable)
include(CableBuildType)
include(CableCompilerSettings)
include(CablePackage)

cable_set_build_type(DEFAULT Release CONFIGURATION_TYPES Debug Release)

project(evmc)
set(PROJECT_VERSION 12.1.0)

set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)  # Must be included after any language is enabled.
if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    # By default put every executable in top-level /bin dir.
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
endif()


cable_configure_compiler(NO_STACK_PROTECTION)
if(CABLE_COMPILER_GNULIKE)
    # TODO: Resolve issues or remove "result optional storage" and enable -Wcast-align.
    add_compile_options(
        -Wmissing-declarations
        $<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast>
    )
    cable_add_cxx_compiler_flag_if_supported(-Wfinal-dtor-non-final-class)
    cable_add_cxx_compiler_flag_if_supported(-Wnewline-eof)
    cable_add_cxx_compiler_flag_if_supported(-Wsuggest-destructor-override)
    cable_add_cxx_compiler_flag_if_supported(-Wunreachable-code-break)
    cable_add_cxx_compiler_flag_if_supported(-Wduplicated-cond)
    cable_add_cxx_compiler_flag_if_supported(-Wduplicate-enum)
    cable_add_cxx_compiler_flag_if_supported(-Wlogical-op)
endif()
if(CABLE_COMPILER_CLANG)
    set(CMAKE_C_FLAGS_COVERAGE "-fprofile-instr-generate -fcoverage-mapping")
    set(CMAKE_CXX_FLAGS_COVERAGE "-fprofile-instr-generate -fcoverage-mapping")
elseif(CABLE_COMPILER_GNU)
    set(CMAKE_C_FLAGS_COVERAGE "--coverage")
    set(CMAKE_CXX_FLAGS_COVERAGE "--coverage")
endif()

set(EVMC_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)

add_subdirectory(lib)

if(EVMC_TOOLS)
    add_subdirectory(tools/evmc)
endif()
