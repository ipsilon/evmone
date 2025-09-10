include_guard()
include(ExternalProject)

if(MSVC)
    set(BLST_BUILD_SCRIPT build.bat)
else()
    # Build CC compiler invocation.
    set(BLST_CC ${CMAKE_C_COMPILER})
    if(CMAKE_OSX_SYSROOT)
        set(BLST_CC "${BLST_CC} ${CMAKE_C_SYSROOT_FLAG} ${CMAKE_OSX_SYSROOT}")
    endif()
    if(CMAKE_C_OSX_DEPLOYMENT_TARGET_FLAG AND CMAKE_OSX_DEPLOYMENT_TARGET)
        set(BLST_CC "${BLST_CC} ${CMAKE_C_OSX_DEPLOYMENT_TARGET_FLAG}${CMAKE_OSX_DEPLOYMENT_TARGET}")
    endif()
    if(CMAKE_C_FLAGS)
        # Pass CFLAGS as part of CC (e.g. to pass -m32). Using CFLAGS directly overwrites blst's default.
        set(BLST_CC "${BLST_CC} ${CMAKE_C_FLAGS}")
    endif()

    set(BLST_BUILD_SCRIPT ./build.sh CC='${BLST_CC}' AR='${CMAKE_AR}' RANLIB='${CMAKE_RANLIB}')
endif()

ExternalProject_Add(
    blst
    EXCLUDE_FROM_ALL TRUE
    PREFIX ${PROJECT_BINARY_DIR}/deps
    URL https://github.com/supranational/blst/archive/refs/tags/v0.3.15.tar.gz
    URL_HASH SHA256=9e503ff6b50e044efb075d260c81c751702b3ed6f2e45394b0833834e71c3afa
    DOWNLOAD_NO_PROGRESS TRUE
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ${BLST_BUILD_SCRIPT}
    BUILD_IN_SOURCE TRUE
    BUILD_BYPRODUCTS "<SOURCE_DIR>/${CMAKE_STATIC_LIBRARY_PREFIX}blst${CMAKE_STATIC_LIBRARY_SUFFIX}"
    LOG_BUILD TRUE
    LOG_OUTPUT_ON_FAILURE TRUE
    INSTALL_COMMAND ""
)
ExternalProject_Get_Property(blst SOURCE_DIR)

set(BLST_INCLUDE_DIR ${SOURCE_DIR}/bindings)
file(MAKE_DIRECTORY ${BLST_INCLUDE_DIR})

add_library(blst::blst STATIC IMPORTED GLOBAL)
add_dependencies(blst::blst blst)
set_target_properties(
    blst::blst PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${BLST_INCLUDE_DIR}
    IMPORTED_LOCATION ${SOURCE_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}blst${CMAKE_STATIC_LIBRARY_SUFFIX}
)

