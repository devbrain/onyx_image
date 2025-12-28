# onyx_image

A modern C++20 library for decoding retro and vintage image formats, with a focus on classic gaming and home computer graphics.

## Features

- **30+ image formats** from vintage computers and gaming platforms
- **Unified API** with automatic format detection
- **Extensible codec registry** for adding custom formats
- **Standard palettes** for CGA, EGA, VGA, C64, Amiga, and Atari ST
- **Raw format decoders** for EGA planar and VGA Mode X data
- **Security hardened** with dimension limits and decompression bomb protection
- **Zero external runtime dependencies** (all dependencies fetched at build time)

## Supported Formats

### Standard Formats
| Format | Extensions | Description |
|--------|------------|-------------|
| PNG | `.png` | Portable Network Graphics |
| JPEG | `.jpg`, `.jpeg` | Joint Photographic Experts Group |
| GIF | `.gif` | Graphics Interchange Format |
| BMP | `.bmp`, `.dib` | Windows/OS2 Bitmap |
| TGA | `.tga`, `.targa` | Truevision TGA |
| PCX | `.pcx` | ZSoft Paintbrush |
| QOI | `.qoi` | Quite OK Image Format |

### Retro PC Formats
| Format | Extensions | Description |
|--------|------------|-------------|
| LBM/IFF | `.lbm`, `.iff`, `.ilbm` | Interleaved Bitmap (Amiga) |
| ICO/CUR | `.ico`, `.cur` | Windows Icon/Cursor |
| DCX | `.dcx` | Multi-page PCX |
| PNM | `.pbm`, `.pgm`, `.ppm` | Portable Anymap |
| SGI | `.sgi`, `.rgb`, `.bw` | Silicon Graphics Image |
| Sun Raster | `.ras`, `.sun` | Sun Microsystems Raster |
| Pictor | `.pic` | PCPaint/Pictor |
| MSP | `.msp` | Microsoft Paint (v1/v2) |

### Commodore 64 Formats
| Format | Extensions | Description |
|--------|------------|-------------|
| Koala | `.koa`, `.kla` | Koala Painter |
| Doodle | `.dd`, `.jj` | Doodle |
| Drazlace | `.drl` | Drazlace (interlaced) |
| Interpaint | `.iph`, `.ipt` | Interpaint Hi-Res/Multicolor |
| AFLI | `.afli`, `.ami` | Advanced FLI |
| FunPaint | `.fpm`, `.fp2`, `.fph` | FunPaint II |
| HiRes | `.hir`, `.hbm` | C64 Hi-Res Bitmap |
| Run Paint | `.rpm` | Run Paint |

### Atari ST Formats
| Format | Extensions | Description |
|--------|------------|-------------|
| NEOchrome | `.neo` | NEOchrome |
| Degas | `.pi1`, `.pi2`, `.pi3`, `.pc1`, `.pc2`, `.pc3` | Degas/Degas Elite |
| Doodle | `.doo` | Atari ST Doodle |
| Crack Art | `.ca1`, `.ca2`, `.ca3` | Crack Art |
| Tiny Stuff | `.tny`, `.tn1`, `.tn2`, `.tn3` | Tiny Stuff |
| Spectrum 512 | `.spu`, `.spc` | Spectrum 512 |
| Photochrome | `.pcs` | Photochrome |

### Raw Formats
| Format | Description |
|--------|-------------|
| EGA Raw | Raw EGA planar data (graphic-planar, row-planar, byte-planar, linear) |
| Mode X Raw | Raw VGA Mode X data (unchained 256-color) |

## Requirements

- CMake 3.20+
- C++20 compiler (GCC 11+, Clang 14+, MSVC 2022+)

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

### CMake Integration

#### FetchContent
```cmake
include(FetchContent)
FetchContent_Declare(onyx_image
    GIT_REPOSITORY https://github.com/devbrain/onyx_image.git
    GIT_TAG main
)
FetchContent_MakeAvailable(onyx_image)

target_link_libraries(your_target PRIVATE neutrino::onyx_image)
```

#### find_package
```cmake
find_package(onyx_image REQUIRED CONFIG)
target_link_libraries(your_target PRIVATE neutrino::onyx_image)
```

### Basic Decoding

```cpp
#include <onyx_image/onyx_image.hpp>
#include <fstream>
#include <vector>

// Load file into memory
std::ifstream file("image.pcx", std::ios::binary);
std::vector<std::uint8_t> data(std::istreambuf_iterator<char>(file), {});

// Decode with automatic format detection
onyx_image::memory_surface surface;
auto result = onyx_image::decode(data, surface);

if (result) {
    // Access pixel data
    auto pixels = surface.pixels();      // std::span<const uint8_t>
    int width = surface.width();
    int height = surface.height();
    auto format = surface.format();      // indexed8, rgb888, or rgba8888

    // For indexed formats, access palette
    auto palette = surface.palette();    // RGB triplets
}
```

### Decode Options

```cpp
onyx_image::decode_options options;
options.max_width = 4096;    // Reject images wider than this
options.max_height = 4096;   // Reject images taller than this

auto result = onyx_image::decode(data, surface, options);
if (!result) {
    std::cerr << "Decode failed: " << result.message << "\n";
    // result.error contains decode_error enum
}
```

### Explicit Codec Selection

```cpp
// Decode using a specific codec
auto result = onyx_image::decode(data, surface, "koala");

// Or use codec directly
if (onyx_image::koala_decoder::sniff(data)) {
    auto result = onyx_image::koala_decoder::decode(data, surface, options);
}
```

### Listing Available Codecs

```cpp
const auto& registry = onyx_image::codec_registry::instance();
for (std::size_t i = 0; i < registry.decoder_count(); ++i) {
    const auto* decoder = registry.decoder_at(i);
    std::cout << decoder->name() << ": ";
    for (const auto& ext : decoder->extensions()) {
        std::cout << ext << " ";
    }
    std::cout << "\n";
}
```

### Standard Palettes

```cpp
#include <onyx_image/palettes.hpp>

// Get standard palettes (constexpr, zero-cost)
auto cga = onyx_image::cga_palette();           // 16 colors
auto ega = onyx_image::ega_default_palette();   // 16 colors
auto vga = onyx_image::vga_default_palette();   // 256 colors
auto c64 = onyx_image::c64_palette();           // 16 colors (Pepto)

// Amiga Workbench palettes
auto wb1 = onyx_image::amiga_wb1_palette();     // 4 colors
auto wb2 = onyx_image::amiga_wb2_palette();     // 4 colors
auto wb3 = onyx_image::amiga_wb3_palette();     // 8 colors

// Atari ST
auto st = onyx_image::atarist_default_palette(); // 16 colors

// Color conversion utilities
auto rgb = onyx_image::ega_color_to_rgb(0x3F);       // 6-bit EGA to RGB888
auto rgb = onyx_image::amiga_color_to_rgb(0x0F00);   // 12-bit Amiga to RGB888
auto rgb = onyx_image::atarist_color_to_rgb(0x777);  // 9-bit ST to RGB888
```

### Raw Format Decoding

```cpp
#include <onyx_image/codecs/ega_raw.hpp>
#include <onyx_image/codecs/modex_raw.hpp>

// Decode raw EGA data
onyx_image::ega_raw_options ega_opts;
ega_opts.width = 320;
ega_opts.height = 200;
ega_opts.format = onyx_image::ega_format::row_planar;
ega_opts.num_planes = 4;

onyx_image::decode_ega_raw(data, surface, ega_opts);

// Decode raw Mode X data
onyx_image::modex_raw_options modex_opts;
modex_opts.width = 320;
modex_opts.height = 240;
modex_opts.format = onyx_image::modex_format::graphic_planar;

onyx_image::decode_modex_raw(data, surface, modex_opts);
```

### PNG Encoding

```cpp
#include <onyx_image/codecs/png.hpp>

// Encode surface to PNG
std::vector<std::uint8_t> png_data = onyx_image::encode_png(surface);

// Save directly to file
onyx_image::save_png(surface, "output.png");
```

## API Reference

### Core Types

```cpp
namespace onyx_image {
    // Pixel formats
    enum class pixel_format { indexed8, rgb888, rgba8888 };

    // Decode errors
    enum class decode_error {
        none, invalid_format, unsupported_version, unsupported_encoding,
        unsupported_bit_depth, dimensions_exceeded, truncated_data,
        io_error, internal_error
    };

    // Decode result
    struct decode_result {
        bool ok;
        decode_error error;
        std::string message;
        explicit operator bool() const;
    };

    // Decode options
    struct decode_options {
        int max_width = 16384;
        int max_height = 16384;
    };
}
```

### Surface Interface

```cpp
class memory_surface {
    // Dimensions
    int width() const;
    int height() const;
    pixel_format format() const;
    std::size_t pitch() const;

    // Read-only pixel access
    std::span<const std::uint8_t> pixels() const;
    std::span<const std::uint8_t> palette() const;

    // Mutable access (for post-processing)
    std::span<std::uint8_t> mutable_pixels();
    std::span<std::uint8_t> mutable_palette();
};
```

### Codec Registry

```cpp
class codec_registry {
    static codec_registry& instance();

    void register_decoder(std::unique_ptr<decoder> dec);
    const decoder* find_decoder(std::span<const std::uint8_t> data) const;
    const decoder* find_decoder(std::string_view name) const;

    std::size_t decoder_count() const;
    const decoder* decoder_at(std::size_t index) const;
};
```

## Architecture

- **Declarative format parsing** using DataScript for binary format specifications
- **Codec registry** with automatic format detection via magic bytes
- **Surface abstraction** supporting indexed, RGB, and RGBA pixel formats
- **Shared utilities** for dimension validation, byte I/O, and row copying

## License

MIT License - see LICENSE file for details.

## Acknowledgments

- [stb_image](https://github.com/nothings/stb) - JPEG, TGA, GIF decoding
- [lodepng](https://github.com/lvandeve/lodepng) - PNG encoding/decoding
- [libiff](https://github.com/svanderburg/libiff) - IFF/ILBM parsing
- Format specifications from various sources including FileFormats.Wiki and ModdingWiki
