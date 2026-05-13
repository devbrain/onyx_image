# ============================================================================
# Dependencies
# ============================================================================

include(${NEUTRINO_CMAKE_DIR}/deps/libiff.cmake)
neutrino_fetch_libiff()

include(${NEUTRINO_CMAKE_DIR}/deps/mz-explode.cmake)
neutrino_fetch_mzexplode()

include(${NEUTRINO_CMAKE_DIR}/deps/datascript.cmake)
neutrino_fetch_datascript()

# stb - single-file public domain libraries
FetchContent_Declare(stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        DOWNLOAD_ONLY YES
)
FetchContent_MakeAvailable(stb)

add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE $<BUILD_INTERFACE:${stb_SOURCE_DIR}>)

# ============================================================================
# Library Target
# ============================================================================

neutrino_library_type(onyx_image _lib_type)

add_library(onyx_image ${_lib_type})
add_library(neutrino::onyx_image ALIAS onyx_image)

target_compile_features(onyx_image PUBLIC cxx_std_20)

target_include_directories(onyx_image
        PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(onyx_image
        PRIVATE
        neutrino::iff
        neutrino::mzexplode
)

# Generate export header
generate_export_header(onyx_image
        BASE_NAME onyx_image
        EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/include/onyx_image/onyx_image_export.h
)

# Library properties
set_target_properties(onyx_image PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        POSITION_INDEPENDENT_CODE ON
)

# ============================================================================
# DataScript Code Generation
# ============================================================================

# DataScript schemas are gathered conditionally per codec axis so we
# don't host-build parsers for codecs that aren't included.
set(_onyx_image_ds_schemas "")
if (NEUTRINO_ONYX_IMAGE_CODEC_PCX)
    list(APPEND _onyx_image_ds_schemas ${CMAKE_CURRENT_SOURCE_DIR}/formats/pcx.ds)
endif ()
if (NEUTRINO_ONYX_IMAGE_CODEC_LBM)
    list(APPEND _onyx_image_ds_schemas ${CMAKE_CURRENT_SOURCE_DIR}/formats/lbm.ds)
endif ()
if (NEUTRINO_ONYX_IMAGE_CODEC_BMP)
    list(APPEND _onyx_image_ds_schemas ${CMAKE_CURRENT_SOURCE_DIR}/formats/bmp.ds)
endif ()
if (NEUTRINO_ONYX_IMAGE_CODEC_SUNRAST)
    list(APPEND _onyx_image_ds_schemas ${CMAKE_CURRENT_SOURCE_DIR}/formats/sunrast.ds)
endif ()
if (NEUTRINO_ONYX_IMAGE_CODEC_PICTOR)
    list(APPEND _onyx_image_ds_schemas ${CMAKE_CURRENT_SOURCE_DIR}/formats/pictor.ds)
endif ()
if (NEUTRINO_ONYX_IMAGE_CODEC_SGI)
    list(APPEND _onyx_image_ds_schemas ${CMAKE_CURRENT_SOURCE_DIR}/formats/sgi.ds)
endif ()
if (NEUTRINO_ONYX_IMAGE_CODEC_MSP)
    list(APPEND _onyx_image_ds_schemas ${CMAKE_CURRENT_SOURCE_DIR}/formats/msp.ds)
endif ()
if (NEUTRINO_ONYX_IMAGE_CODEC_ATARIST)
    list(APPEND _onyx_image_ds_schemas ${CMAKE_CURRENT_SOURCE_DIR}/formats/atarist.ds)
endif ()

if (_onyx_image_ds_schemas)
    datascript_generate(
            TARGET onyx_image_parsers
            SCHEMAS ${_onyx_image_ds_schemas}
            OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated
            IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
            INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}
            PRESERVE_PACKAGE_DIRS ON
    )
endif ()

# ============================================================================
# STB Wrapper (compiled with warnings disabled)
# ============================================================================
# Built only when CODEC_STB is enabled — wraps JPEG/TGA/GIF via stb_image.

if (NEUTRINO_ONYX_IMAGE_CODEC_STB)
    add_library(onyx_image_stb OBJECT codecs/stb.cpp)
    target_include_directories(onyx_image_stb PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    )
    target_link_libraries(onyx_image_stb PRIVATE stb_image)
    target_compile_features(onyx_image_stb PRIVATE cxx_std_20)
    if (NEUTRINO_ONYX_IMAGE_BUILD_SHARED)
        set_target_properties(onyx_image_stb PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif ()
    # Disable warnings for stb_image wrapper (cross-platform)
    target_compile_options(onyx_image_stb PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/W0>
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>
    )
endif ()

# ============================================================================
# LodePNG (embedded single-file PNG library)
# ============================================================================
# Built only when CODEC_PNG is enabled — png.cpp delegates the actual
# encode/decode to lodepng.

if (NEUTRINO_ONYX_IMAGE_CODEC_PNG)
    add_library(onyx_image_lodepng OBJECT thirdparty/lodepng/lodepng.cpp)
    target_include_directories(onyx_image_lodepng PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lodepng>
    )
    target_compile_features(onyx_image_lodepng PRIVATE cxx_std_20)
    if (NEUTRINO_ONYX_IMAGE_BUILD_SHARED)
        set_target_properties(onyx_image_lodepng PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif ()
    # Disable warnings for lodepng (cross-platform)
    target_compile_options(onyx_image_lodepng PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/W0>
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>
    )
endif ()

# ============================================================================
# Library Sources
# ============================================================================
# Always-built core (types / surface / palettes / codec registry).
target_sources(onyx_image PRIVATE
        types.cpp
        surface.cpp
        palettes.cpp
        codec.cpp
)

# Per-codec source files — each gated by NEUTRINO_ONYX_IMAGE_CODEC_<NAME>.
# The corresponding registration in codec.cpp is #ifdef'd via the same
# ONYX_IMAGE_HAS_<NAME> compile defs emitted at the bottom of this file.
foreach (_codec_pair IN ITEMS
        "LBM;codecs/lbm.cpp"
        "PCX;codecs/pcx.cpp"
        "PNG;codecs/png.cpp"
        "BMP;codecs/bmp.cpp"
        "QOI;codecs/qoi.cpp"
        "ICO;codecs/ico.cpp"
        "XPM;codecs/xpm.cpp"
        "SUNRAST;codecs/sunrast.cpp"
        "SGI;codecs/sgi.cpp"
        "PNM;codecs/pnm.cpp"
        "DCX;codecs/dcx.cpp"
        "MSP;codecs/msp.cpp"
        "PICTOR;codecs/pictor.cpp"
        "ATARIST;codecs/atarist.cpp"
        "AMI;codecs/ami.cpp"
        "KOALA;codecs/koala.cpp"
        "C64_DOODLE;codecs/c64_doodle.cpp"
        "C64_HIRES;codecs/c64_hires.cpp"
        "DRAZLACE;codecs/drazlace.cpp"
        "FUNPAINT;codecs/funpaint.cpp"
        "RUNPAINT;codecs/runpaint.cpp"
        "INTERPAINT;codecs/interpaint.cpp"
        "EGA_RAW;codecs/ega_raw.cpp"
        "MODEX_RAW;codecs/modex_raw.cpp")
    list(GET _codec_pair 0 _codec)
    list(GET _codec_pair 1 _src)
    if (NEUTRINO_ONYX_IMAGE_CODEC_${_codec})
        target_sources(onyx_image PRIVATE ${_src})
        target_compile_definitions(onyx_image PRIVATE ONYX_IMAGE_HAS_${_codec}=1)
    endif ()
endforeach ()

# STB wrapper (jpeg/tga/gif) and lodepng (png) are object libs with
# their own warning suppression; only linked into onyx_image when their
# corresponding codec axis is enabled.
#
# The matching ONYX_IMAGE_HAS_<CODEC>=1 compile definitions gate the
# codec registration blocks in codec.cpp. PNG already gets its define
# via the foreach loop above (PNG is in the per-codec list), but STB is
# NOT in that list — without explicitly adding the define here, the
# JPEG/TGA/GIF decoders in codec.cpp stay #ifdef'd out, so decode()
# returns "Unknown image format" even though stb_image is linked in.
if (NEUTRINO_ONYX_IMAGE_CODEC_STB)
    target_sources(onyx_image PRIVATE $<TARGET_OBJECTS:onyx_image_stb>)
    target_compile_definitions(onyx_image PRIVATE ONYX_IMAGE_HAS_STB=1)
endif ()
if (NEUTRINO_ONYX_IMAGE_CODEC_PNG)
    target_sources(onyx_image PRIVATE $<TARGET_OBJECTS:onyx_image_lodepng>)
endif ()

target_include_directories(onyx_image PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lodepng
)

# onyx_image_parsers is conditionally created above (only if at least
# one ds-using codec is enabled). Link if it exists.
if (TARGET onyx_image_parsers)
    target_link_libraries(onyx_image PRIVATE onyx_image_parsers)
endif ()
