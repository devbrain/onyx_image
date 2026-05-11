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

# -----------------------------------------------------------------------------
# Codec selection
# -----------------------------------------------------------------------------
# onyx_image ships ~25 image format decoders. Most consumers only need a
# few — warlords (W1/W2/W2D) needs LBM, PCX, PNG, BMP. Size-conscious
# downstreams can bulk-disable then opt back in:
#
#   set(NEUTRINO_ONYX_IMAGE_CODEC_ALL OFF CACHE BOOL "" FORCE)
#   set(NEUTRINO_ONYX_IMAGE_CODEC_LBM ON  CACHE BOOL "" FORCE)
#   set(NEUTRINO_ONYX_IMAGE_CODEC_PCX ON  CACHE BOOL "" FORCE)
#   set(NEUTRINO_ONYX_IMAGE_CODEC_PNG ON  CACHE BOOL "" FORCE)
#   set(NEUTRINO_ONYX_IMAGE_CODEC_BMP ON  CACHE BOOL "" FORCE)
#
# Defaults to ALL ON for backward compatibility. Each axis controls
# its source .cpp (src/codecs/<name>.cpp), the corresponding _impl
# class + registration in codec.cpp (gated via ONYX_IMAGE_HAS_<NAME>
# compile defs), and where applicable the datascript schema generation
# and helper object libraries (lodepng for PNG, stb_image for STB).
#
# Bundled-codec axes:
#   STB     — JPEG, TGA, GIF (single stb.cpp wrapper)
#   ATARIST — neo, degas, doodle, crack_art, tiny_stuff,
#             spectrum512, photochrome (single atarist.cpp)
#   ICO     — ico + exe_icon (uses mzexplode's PE reader for exe_icon)

neutrino_option_group(NEUTRINO_ONYX_IMAGE_CODEC_ALL
    "Default state for all onyx_image codecs" ON)

# Always-useful / common codecs
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_LBM
    "LBM/ILBM (IFF, Deluxe Paint, Amiga)" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_PCX
    "PCX (ZSoft Paintbrush)" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_PNG
    "PNG via lodepng" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_BMP
    "BMP (Windows Bitmap)" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_STB
    "STB family — JPEG, TGA, GIF (bundled via stb_image)" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_QOI
    "QOI (Quite OK Image format)" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_ICO
    "ICO + exe_icon (PE icon extraction via mzexplode)" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_XPM
    "XPM (X11 Pixmap)" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)

# Workstation / scanner formats
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_SUNRAST
    "SunRaster (Sun)" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_SGI
    "SGI RGB" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_PNM
    "PNM/PBM/PGM/PPM (Netpbm)" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_DCX
    "DCX (multi-page PCX fax)" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_MSP
    "MSP (Microsoft Paint v1/v2)" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_PICTOR
    "Pictor (PC Paint / PIC format)" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)

# Atari/Amiga/C64 retro formats — rarely needed by general consumers
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_ATARIST
    "Atari ST family — neo/degas/doodle/crack_art/tiny_stuff/spectrum512/photochrome" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_AMI
    "Amiga IFF (single-image)" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_KOALA
    "C64 Koala Painter" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_C64_DOODLE
    "C64 Doodle" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_C64_HIRES
    "C64 Hi-Res (Art Studio)" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_DRAZLACE
    "C64 Drazlace" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_FUNPAINT
    "C64 Funpaint" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_RUNPAINT
    "C64 Runpaint" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_INTERPAINT
    "C64 Interpaint" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)

# Explicit raw-buffer decoders (not auto-detected, used directly by
# callers that already know the geometry)
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_EGA_RAW
    "EGA raw planar buffer (explicit-API)" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)
neutrino_option(NEUTRINO_ONYX_IMAGE_CODEC_MODEX_RAW
    "Mode-X raw chunky buffer (explicit-API)" ON
    GROUP NEUTRINO_ONYX_IMAGE_CODEC_ALL)



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
    neutrino_print_summary(onyx_image)
endif ()
