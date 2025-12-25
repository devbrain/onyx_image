cmake_minimum_required(VERSION 3.20)

project(onyx_image
    VERSION 0.1.0
    DESCRIPTION "Image format loading library for retro game formats"
    LANGUAGES CXX
)

# ============================================================================
# Neutrino CMake Integration
# ============================================================================

include(FetchContent)
include(GenerateExportHeader)

if(NOT DEFINED NEUTRINO_CMAKE_DIR)
    FetchContent_Declare(neutrino_cmake
        GIT_REPOSITORY https://github.com/devbrain/neutrino-cmake.git
        GIT_TAG master
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(neutrino_cmake)
    set(NEUTRINO_CMAKE_DIR "${neutrino_cmake_SOURCE_DIR}/cmake")
    list(APPEND CMAKE_MODULE_PATH "${NEUTRINO_CMAKE_DIR}")
endif()

include(NeutrinoInit)

# ============================================================================
# Options
# ============================================================================

neutrino_define_library_options(onyx_image)

# ============================================================================
# Dependencies
# ============================================================================

include(${NEUTRINO_CMAKE_DIR}/deps/libiff.cmake)
neutrino_fetch_libiff()

include(${NEUTRINO_CMAKE_DIR}/deps/mz-explode.cmake)
neutrino_fetch_mzexplode()

include(${NEUTRINO_CMAKE_DIR}/deps/datascript.cmake)
neutrino_fetch_datascript()


# ============================================================================
# Library Target
# ============================================================================

neutrino_library_type(onyx_image _lib_type)

add_library(onyx_image ${_lib_type}
    src/onyx_image.cpp
)
add_library(neutrino::onyx_image ALIAS onyx_image)

target_compile_features(onyx_image PUBLIC cxx_std_20)

target_include_directories(onyx_image
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(onyx_image PUBLIC
    neutrino::iff
    neutrino::mzexplode
    neutrino::datascript
)

# Generate export header
generate_export_header(onyx_image
    BASE_NAME onyx_image
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/include/onyx_image/onyx_image_export.h
)

# ============================================================================
# Compiler Warnings
# ============================================================================

neutrino_target_warnings(onyx_image)
neutrino_target_sanitizers(onyx_image)

# ============================================================================
# Tests
# ============================================================================

if(NEUTRINO_ONYX_IMAGE_BUILD_TESTS)
    include(${NEUTRINO_CMAKE_DIR}/deps/doctest.cmake)
    neutrino_fetch_doctest()

    enable_testing()
    add_subdirectory(test)
endif()

# ============================================================================
# Examples
# ============================================================================

if(NEUTRINO_ONYX_IMAGE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ============================================================================
# Installation
# ============================================================================

# Note: Full CMake export/install is not supported when onyx_image is built with
# FetchContent dependencies. Those targets cannot be exported.
# For installation, build against system-installed dependencies.

# ============================================================================
# Summary
# ============================================================================

neutrino_is_top_level(_is_top_level)
if(_is_top_level)
    neutrino_print_options(onyx_image)
endif()
