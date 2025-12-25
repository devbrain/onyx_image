# onyx_image

Image format loading library for retro game formats

## Requirements

- CMake 3.20+
- C++20 compiler

## Building

```bash
cmake -B build
cmake --build build
```

## Testing

```bash
cmake -B build -DNEUTRINO_ONYX_IMAGE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

## Installation

```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
cmake --install build
```

## Usage

### FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(onyx_image
    GIT_REPOSITORY https://github.com/devbrain/onyx_image.git
    GIT_TAG main
)
FetchContent_MakeAvailable(onyx_image)

target_link_libraries(your_target PRIVATE neutrino::onyx_image)
```

### find_package

```cmake
find_package(onyx_image REQUIRED CONFIG)
target_link_libraries(your_target PRIVATE neutrino::onyx_image)
```

## License

MIT License - see LICENSE file for details.
