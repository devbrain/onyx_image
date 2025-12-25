add_executable(onyx_image_tests
    test_main.cpp
    test_onyx_image.cpp
)

target_link_libraries(onyx_image_tests PRIVATE
    onyx_image
    doctest::doctest
)

neutrino_target_warnings(onyx_image_tests)
neutrino_target_sanitizers(onyx_image_tests)

include(CTest)
add_test(NAME onyx_image_tests COMMAND onyx_image_tests)
