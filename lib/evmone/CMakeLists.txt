# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2019 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

include(LibraryTools)

add_library(evmone
    ${include_dir}/evmone/evmone.h
    advanced_analysis.cpp
    advanced_analysis.hpp
    advanced_execution.cpp
    advanced_execution.hpp
    advanced_instructions.cpp
    baseline.hpp
    baseline_analysis.cpp
    baseline_execution.cpp
    baseline_instruction_table.cpp
    baseline_instruction_table.hpp
    constants.hpp
    create_address.cpp
    create_address.hpp
    delegation.cpp
    delegation.hpp
    execution_context.hpp
    instructions.hpp
    instructions_calls.cpp
    instructions_opcodes.hpp
    instructions_storage.cpp
    instructions_traits.hpp
    instructions_xmacro.hpp
    lru_cache.hpp
    tracing.cpp
    tracing.hpp
    vm.cpp
    vm.hpp
)
target_compile_features(evmone PUBLIC cxx_std_20)
target_link_libraries(evmone PUBLIC evmc::evmc intx::intx PRIVATE evmone::precompiles)

# Nested EVM calls recurse ~2.6 KB of stack per frame, too much for Windows' 1 MB default.
target_link_options(evmone INTERFACE $<$<CXX_COMPILER_ID:MSVC>:/STACK:8388608>)

target_include_directories(evmone PUBLIC
    $<BUILD_INTERFACE:${include_dir}>$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(EVMONE_X86_64_ARCH_LEVEL GREATER_EQUAL 2)
    # Add CPU architecture runtime check. The EVMONE_X86_64_ARCH_LEVEL has a valid value.
    target_sources(evmone PRIVATE cpu_check.cpp)
    set_source_files_properties(cpu_check.cpp PROPERTIES COMPILE_DEFINITIONS EVMONE_X86_64_ARCH_LEVEL=${EVMONE_X86_64_ARCH_LEVEL})
endif()

if(CABLE_COMPILER_GNULIKE)
    target_compile_options(
        evmone PRIVATE
        -fno-exceptions
        $<$<CXX_COMPILER_ID:GNU>:-Wstack-usage=2900>
    )
    if(NOT SANITIZE MATCHES undefined)
        # RTTI can be disabled except for UBSan which checks vptr integrity.
        target_compile_options(evmone PRIVATE -fno-rtti)
    endif()

    # On Linux x86-64 build with -fcf-protection=return: only the shadow stack part of Intel CET.
    # The branch part (IBT), which e.g. Ubuntu's GCC enables by default (-fcf-protection=full), puts
    # an ENDBR64 at every indirect branch target: at each instruction of the Baseline interpreter,
    # executed on every dispatch, while Linux does not enforce IBT in user space. The shadow stack
    # costs no instructions, and the linker marks a binary shadow-stack compatible only if all its
    # objects are. A -fcf-protection in the CMake flags (of any configuration) is kept.
    # TODO: Keep the compiler's default when it has no branch part instead of always setting it.
    if(CMAKE_SYSTEM_NAME STREQUAL Linux AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
        set(flags "${CMAKE_CXX_FLAGS}")
        if(CMAKE_CONFIGURATION_TYPES)
            set(configs ${CMAKE_CONFIGURATION_TYPES})
        else()
            set(configs ${CMAKE_BUILD_TYPE})
        endif()
        foreach(config IN LISTS configs)
            string(TOUPPER ${config} config)
            string(APPEND flags " ${CMAKE_CXX_FLAGS_${config}}")
        endforeach()
        if(NOT flags MATCHES "-fcf-protection")
            # The option is in GCC 8 and Clang 7, older than the minimum compilers (see circle.yml).
            target_compile_options(evmone PRIVATE -fcf-protection=return)
        endif()
    endif()
endif()
set_target_properties(
    evmone
    PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_SOVERSION}
)

if(NOT SANITIZE)
    # On Linux, check if all symbols in evmone are resolved during linking.
    target_link_options(evmone PRIVATE $<$<PLATFORM_ID:Linux>:LINKER:--no-undefined>)
endif()

set_source_files_properties(vm.cpp PROPERTIES COMPILE_DEFINITIONS PROJECT_VERSION="${PROJECT_VERSION}")

add_standalone_library(evmone)
