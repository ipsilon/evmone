# evmone: Ethereum Virtual Machine
# Copyright 2019 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22...4.2)

option(BUILD_SHARED_LIBS "Build evmone as a shared library" ON)
option(COVERAGE "Build with coverage instrumentation" OFF)
option(ASSERTIONS "Force enable the assertions in the code ignoring the CMAKE_BUILD_TYPE default" OFF)
option(EVMONE_TESTING "Build tests and test tools" OFF)
option(EVMONE_FUZZING "Instrument libraries and build fuzzing tools" OFF)

include(cmake/cable/bootstrap.cmake)
include(CableBuildType)
include(CableCompilerSettings)
include(CablePackage)
include(CableToolchains)
include(CMakePackageConfigHelpers)

cable_configure_toolchain(DEFAULT cxx17-pic)
cable_set_build_type(DEFAULT Release CONFIGURATION_TYPES Release Debug)

include(Hunter/init)

project(evmone LANGUAGES CXX C)
set(PROJECT_VERSION 0.21.0)

option(EVMONE_TOOLS "Build evmone tools" ${PROJECT_IS_TOP_LEVEL})

string(REGEX MATCH "([0-9]+)\\.([0-9]+)" _ ${PROJECT_VERSION})
set(PROJECT_VERSION_MAJOR ${CMAKE_MATCH_1})
set(PROJECT_VERSION_MINOR ${CMAKE_MATCH_2})

set(PROJECT_SOVERSION ${PROJECT_VERSION_MAJOR})
if(PROJECT_VERSION_MAJOR EQUAL 0)
    set(PROJECT_SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})
endif()

if(COMMAND block) # TODO(cmake-3.25)
    block()
        set(CMAKE_C_CLANG_TIDY "")
        set(CMAKE_CXX_CLANG_TIDY "")
        add_subdirectory(evmc)
    endblock()
else()
    add_subdirectory(evmc)
endif()

cable_configure_compiler(NO_STACK_PROTECTION)
if(CABLE_COMPILER_GNULIKE)
    add_compile_options(
        -Wmissing-declarations
        $<$<COMPILE_LANGUAGE:CXX>:-Wextra-semi>
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-missing-field-initializers>

        $<$<CXX_COMPILER_ID:GNU>:-Wno-attributes>
        $<$<CXX_COMPILER_ID:GNU>:-Wduplicated-cond>
        $<$<CXX_COMPILER_ID:GNU>:-Wlogical-op>

        $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-unknown-attributes>
        $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wduplicate-enum>
        $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wnewline-eof>
        $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wunreachable-code-aggressive>
    )
elseif(MSVC)
    add_compile_options(/wd4324)  # Disabled warning about alignment caused by alignas.
    add_compile_options(/wd5030)  # Allow using unknown attributes.
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    message(FATAL_ERROR "Use -DCOVERAGE=1")
endif()

if(COVERAGE)
    add_compile_options(
        $<$<CXX_COMPILER_ID:GNU>:--coverage>

        # Enable Clang's Source Based Coverage.
        # https://clang.llvm.org/docs/SourceBasedCodeCoverage.html
        $<$<CXX_COMPILER_ID:Clang,AppleClang>:-fprofile-instr-generate>
        $<$<CXX_COMPILER_ID:Clang,AppleClang>:-fcoverage-mapping>
        $<$<CXX_COMPILER_ID:Clang,AppleClang>:-fcoverage-mcdc>
    )
    add_link_options(
        $<$<CXX_COMPILER_ID:GNU>:--coverage>
        $<$<CXX_COMPILER_ID:Clang,AppleClang>:-fprofile-instr-generate>
    )
endif()

if(ASSERTIONS)
    if(CMAKE_CONFIGURATION_TYPES)
        message(FATAL_ERROR "ASSERTIONS option is not supported with multi-configuration generators")
    endif()
    # Remove -DNDEBUG from default build type flags.
    string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)
    get_property(ENABLED_LANGUAGES GLOBAL PROPERTY ENABLED_LANGUAGES)
    foreach(LANG IN LISTS ENABLED_LANGUAGES)
        string(REGEX REPLACE " *-DNDEBUG" "" CMAKE_${LANG}_FLAGS_${BUILD_TYPE_UPPER} "${CMAKE_${LANG}_FLAGS_${BUILD_TYPE_UPPER}}")
    endforeach()
endif()

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL x86_64)
    # Setup options for x86_64 micro-architecture levels.
    # https://clang.llvm.org/docs/UsersManual.html#x86

    set(EVMONE_X86_64_ARCH_LEVEL_INIT 2)
    if(APPLE)
        # On macos with Apple Silicon CPU (arm64) the x86 is emulated and SSE4.2 is not available.
        set(EVMONE_X86_64_ARCH_LEVEL_INIT 1)
    endif()

    set(EVMONE_X86_64_ARCH_LEVEL ${EVMONE_X86_64_ARCH_LEVEL_INIT} CACHE STRING "The x86_64 micro-architecture level")
    if(EVMONE_X86_64_ARCH_LEVEL GREATER_EQUAL 1 AND EVMONE_X86_64_ARCH_LEVEL LESS_EQUAL 4)
        message(STATUS "x86_64 micro-architecture level: ${EVMONE_X86_64_ARCH_LEVEL}")
        if(EVMONE_X86_64_ARCH_LEVEL GREATER_EQUAL 2)
            add_compile_options(-march=x86-64-v${EVMONE_X86_64_ARCH_LEVEL})
        endif()
    else()
        message(FATAL_ERROR "Invalid EVMONE_X86_64_ARCH_LEVEL: ${EVMONE_X86_64_ARCH_LEVEL}")
    endif()
endif()

include(GNUInstallDirs)

if(EVMONE_FUZZING)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build evmone as a shared library" FORCE)
    set(EVMONE_TESTING ON CACHE BOOL "Build tests and test tools" FORCE)

    if(NOT COVERAGE)
        # Add fuzzing instrumentation only for non-coverage builds.
        # The coverage builds should be without fuzzing instrumentation to allow
        # running fuzzing corpus once and getting code coverage.
        set(fuzzing_flags -fsanitize=fuzzer-no-link,address,undefined,shift-exponent,implicit-conversion,nullability)
        add_compile_options(${fuzzing_flags})
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${fuzzing_flags}")
    endif()
endif()

set(EVMONE_INTX_DIR "" CACHE PATH "Path to local intx source directory")
if(EVMONE_INTX_DIR)
    add_subdirectory(${EVMONE_INTX_DIR} intx)
else()
    hunter_add_package(intx)
    find_package(intx CONFIG REQUIRED)
endif()

set(include_dir ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(lib)

if(EVMONE_TOOLS)
    add_subdirectory(tools)
endif()

if(EVMONE_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()


# INSTALL

set(install_targets evmone)
if(TARGET evmone-standalone)
    list(APPEND install_targets evmone-standalone)
endif()
if(TARGET evmone-cli)
    list(APPEND install_targets evmone-cli)
endif()
if(TARGET evmone-bench)
    list(APPEND install_targets evmone-bench)
endif()

set_target_properties(
    ${install_targets} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}
)

install(TARGETS ${install_targets} EXPORT evmoneTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY ${include_dir}/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

cable_add_archive_package()
