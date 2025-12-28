cmake_minimum_required(VERSION 3.20)

project(onyx_image
        VERSION 0.1.0
        DESCRIPTION "Image format loading library for retro game formats"
        LANGUAGES CXX C
)

# ============================================================================
# Neutrino CMake Integration
# ============================================================================

include(FetchContent)
include(GenerateExportHeader)

if (NOT DEFINED NEUTRINO_CMAKE_DIR)
    FetchContent_Declare(neutrino_cmake
            GIT_REPOSITORY https://github.com/devbrain/neutrino-cmake.git
            GIT_TAG master
            GIT_SHALLOW TRUE
            UPDATE_COMMAND ""
    )
    FetchContent_MakeAvailable(neutrino_cmake)
    set(NEUTRINO_CMAKE_DIR "${neutrino_cmake_SOURCE_DIR}/cmake" CACHE PATH "Neutrino CMake modules directory")
endif ()

list(APPEND CMAKE_MODULE_PATH "${NEUTRINO_CMAKE_DIR}")
include(NeutrinoInit)

# ============================================================================
# Options
# ============================================================================

neutrino_define_library_options(onyx_image)



# ============================================================================
# Sources
# ============================================================================

add_subdirectory(src)

# ============================================================================
# Compiler Warnings
# ============================================================================

neutrino_target_warnings(onyx_image)
neutrino_target_sanitizers(onyx_image)

# ============================================================================
# Tests
# ============================================================================

if (NEUTRINO_ONYX_IMAGE_BUILD_TESTS)
    include(${NEUTRINO_CMAKE_DIR}/deps/doctest.cmake)
    neutrino_fetch_doctest()

    enable_testing()
    add_subdirectory(test)
endif ()

# ============================================================================
# Examples
# ============================================================================

if (NEUTRINO_ONYX_IMAGE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

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
if (_is_top_level)
    neutrino_print_options(onyx_image)
endif ()
