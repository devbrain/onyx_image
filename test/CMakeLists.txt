# Disable warnings for third-party md5 implementation
set_source_files_properties(helpers/md5.c PROPERTIES COMPILE_OPTIONS "-w")

add_executable(onyx_image_tests
    test_main.cpp
    test_onyx_image.cpp
    test_pcx_decoder.cpp
    test_lbm_decoder.cpp
    test_bmp_decoder.cpp
    test_sunrast_decoder.cpp
    test_pictor_decoder.cpp
    test_sgi_decoder.cpp
    test_pnm_decoder.cpp
    test_dcx_decoder.cpp
    test_msp_decoder.cpp
    test_atarist_decoder.cpp
    test_ico_decoder.cpp
    test_koala_decoder.cpp
    test_c64_doodle_decoder.cpp
    test_drazlace_decoder.cpp
    test_interpaint_decoder.cpp
    test_ami_decoder.cpp
    test_funpaint_decoder.cpp
    test_c64_hires_decoder.cpp
    test_runpaint_decoder.cpp
    helpers/md5.c
)

target_link_libraries(onyx_image_tests PRIVATE
    onyx_image
    doctest::doctest
)

# Define test data directory
target_compile_definitions(onyx_image_tests PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

neutrino_target_warnings(onyx_image_tests)
neutrino_target_sanitizers(onyx_image_tests)

include(CTest)
add_test(NAME onyx_image_tests COMMAND onyx_image_tests)
