# ============================================================================
# Dependencies
# ============================================================================

include(${NEUTRINO_CMAKE_DIR}/deps/libiff.cmake)
neutrino_fetch_libiff()

include(${NEUTRINO_CMAKE_DIR}/deps/mz-explode.cmake)
neutrino_fetch_mzexplode()

include(${NEUTRINO_CMAKE_DIR}/deps/datascript.cmake)
neutrino_fetch_datascript()

# lodepng - PNG encoder/decoder (single-file library)
FetchContent_Declare(lodepng
        GIT_REPOSITORY https://github.com/lvandeve/lodepng.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        DOWNLOAD_ONLY YES
)
FetchContent_MakeAvailable(lodepng)

add_library(lodepng STATIC ${lodepng_SOURCE_DIR}/lodepng.cpp)
target_include_directories(lodepng PUBLIC $<BUILD_INTERFACE:${lodepng_SOURCE_DIR}>)
target_compile_options(lodepng PRIVATE -w)
set_target_properties(lodepng PROPERTIES POSITION_INDEPENDENT_CODE ON)

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

datascript_generate(
        TARGET onyx_image_parsers
        SCHEMAS
        ${CMAKE_CURRENT_SOURCE_DIR}/formats/pcx.ds
        ${CMAKE_CURRENT_SOURCE_DIR}/formats/lbm.ds
        ${CMAKE_CURRENT_SOURCE_DIR}/formats/bmp.ds
        ${CMAKE_CURRENT_SOURCE_DIR}/formats/sunrast.ds
        ${CMAKE_CURRENT_SOURCE_DIR}/formats/pictor.ds
        ${CMAKE_CURRENT_SOURCE_DIR}/formats/sgi.ds
        ${CMAKE_CURRENT_SOURCE_DIR}/formats/msp.ds
        ${CMAKE_CURRENT_SOURCE_DIR}/formats/atarist.ds
        OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated
        IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}
        PRESERVE_PACKAGE_DIRS ON
)

# ============================================================================
# Library Sources
# ============================================================================

# Disable warnings for stb_image (third-party single-header library)
set_source_files_properties(codecs/stb.cpp PROPERTIES COMPILE_OPTIONS "-w")

target_sources(onyx_image PRIVATE
        types.cpp
        surface.cpp
        palettes.cpp
        codec.cpp
        codecs/pcx.cpp
        codecs/png.cpp
        codecs/lbm.cpp
        codecs/stb.cpp
        codecs/bmp.cpp
        codecs/sunrast.cpp
        codecs/pictor.cpp
        codecs/sgi.cpp
        codecs/pnm.cpp
        codecs/dcx.cpp
        codecs/msp.cpp
        codecs/atarist.cpp
        codecs/qoi.cpp
        codecs/ico.cpp
        codecs/koala.cpp
        codecs/c64_doodle.cpp
        codecs/drazlace.cpp
        codecs/interpaint.cpp
        codecs/ami.cpp
        codecs/funpaint.cpp
        codecs/c64_hires.cpp
        codecs/runpaint.cpp
        codecs/ega_raw.cpp
        codecs/modex_raw.cpp
)

target_include_directories(onyx_image PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(onyx_image PRIVATE
        onyx_image_parsers
        lodepng
        stb_image
)

