add_executable(onyx_image_example
    example.cpp
)

target_link_libraries(onyx_image_example PRIVATE
    onyx_image
)

neutrino_target_warnings(onyx_image_example)
