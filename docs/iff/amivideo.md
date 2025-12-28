# Amiga Video Converter Library - C++17 Design Document

## 1. Executive Summary

This document describes the design of a minimal, efficient C++17 library for converting Amiga video memory formats to modern display representations. The library focuses on accurate conversion of planar graphics, palette handling, and special video modes (HAM, EHB) while maintaining simplicity and performance.

## 2. Terminology and Glossary

### 2.1 Amiga-Specific Terms

**AGA (Advanced Graphics Architecture)**
- Third generation Amiga chipset (1992)
- Supports 8-bit color channels (24-bit RGB)
- Up to 8 bitplanes, 256 colors from 16.7M palette
- Introduced SuperHires mode (1280px horizontal)

**Bitplane**
- A single bit layer of image data
- Each bitplane contributes one bit to the final pixel's color index
- Amiga supports 1-8 bitplanes (2-256 colors)

**Chip RAM**
- Special RAM accessible by custom chips (Agnus/Alice)
- Required for video display, audio, and blitter operations
- Limited to 512KB (OCS), 1MB (ECS), or 2MB (AGA)

**Chunky Pixel Format**
- Modern pixel format where consecutive bytes represent complete pixels
- Each byte contains the full color index for one pixel
- Contrasts with Amiga's native planar format

**Copper**
- Display coprocessor that modifies hardware registers mid-frame
- Enables effects like gradient backgrounds and split screens
- Not simulated by this library

**DMA (Direct Memory Access)**
- Hardware mechanism for chips to access memory without CPU
- Display DMA fetches bitplane data automatically

**Dual Playfield**
- Mode where two independent screens overlay each other
- Each playfield uses alternating bitplanes
- Enables parallax scrolling effects

**ECS (Enhanced Chip Set)**
- Second generation Amiga chipset (1990)
- Added SuperHires and productivity modes
- Maintained 12-bit color (4096 colors)

**EHB (Extra-Halfbrite)**
- 64-color mode using 6 bitplanes
- Colors 32-63 are half-brightness versions of colors 0-31
- OCS/ECS feature, not available on AGA

**HAM (Hold-And-Modify)**
- Special mode displaying many colors simultaneously
- HAM6: 4096 colors using 6 bitplanes (OCS/ECS)
- HAM8: 262,144 colors using 8 bitplanes (AGA)
- Each pixel either uses palette or modifies previous pixel's RGB

**HiRes (High Resolution)**
- 640 pixel horizontal resolution mode
- Pixels have 1:1 aspect ratio on NTSC/PAL displays
- 4 colors maximum without special modes

**ILBM (Interleaved Bitmap)**
- IFF file format for Amiga images
- Stores bitplane data interleaved by scanline
- Must be de-interleaved for processing

**Lace (Interlace)**
- Doubles vertical resolution by alternating scanlines
- Creates flicker on CRT monitors
- Requires line-doubling for modern displays

**LoRes (Low Resolution)**
- 320 pixel horizontal resolution mode
- Pixels have 2:1 aspect ratio (wide)
- Supports up to 32 colors (5 bitplanes) or special modes

**OCS (Original Chip Set)**
- First generation Amiga chipset (1985)
- 12-bit color (4096 colors)
- Up to 6 bitplanes, 32/64 colors or HAM6

**Palette**
- Color lookup table (CLUT)
- Maps color indices to RGB values
- 32 registers (OCS/ECS) or 256 registers (AGA)

**Planar Format**
- Amiga's native graphics format
- Pixel bits distributed across multiple memory planes
- Efficient for hardware but requires conversion for modern systems

**SuperHiRes (Super High Resolution)**
- 1280 pixel horizontal resolution mode
- AGA-only feature
- Maximum 4 colors without tricks

**Viewport**
- Independent display area with own resolution and palette
- Multiple viewports can be stacked vertically
- Each has its own mode settings

**Word Alignment**
- Amiga requires graphics data aligned to 16-bit boundaries
- Each scanline must be padded to even number of bytes
- Critical for DMA operations

### 2.2 General Graphics Terms

**Aspect Ratio**
- Ratio of pixel width to height
- Amiga pixels are non-square (except HiRes)
- Requires correction for modern displays

**Color Channel**
- Individual component of color (Red, Green, Blue)
- OCS/ECS: 4 bits per channel (0-15)
- AGA: 8 bits per channel (0-255)

**Pitch (Stride)**
- Number of bytes per scanline in memory
- May include padding for alignment
- Formula: `((width + 15) / 16) * 2` bytes

**RGBA**
- Red, Green, Blue, Alpha color format
- 32 bits per pixel (8 bits per channel)
- Standard format for modern graphics

**Scanline**
- Single horizontal line of pixels
- Basic unit of display generation
- Processed sequentially by display hardware

## 3. Amiga Video Memory Theory

### 3.1 Historical Context

The Amiga's graphics architecture was revolutionary for 1985, designed by Jay Miner to provide arcade-quality graphics at home computer prices. The system used custom chips to offload graphics operations from the CPU, enabling smooth animation and rich colors that were impossible on contemporary systems.

### 3.2 Planar Graphics Architecture

#### 3.2.1 Why Planar?

The planar format was chosen for several reasons:

1. **Memory Efficiency**: For typical graphics with few colors, planar format uses less memory than chunky
2. **Hardware Simplicity**: Easier to implement color depth flexibility in hardware
3. **Blitter Optimization**: The blitter chip could operate on individual bitplanes efficiently
4. **Bandwidth Conservation**: Allowed selective updating of specific bit layers

#### 3.2.2 Memory Organization

```
Memory Layout for 4-color (2 bitplane) 32x16 image:

Bitplane 0:                    Bitplane 1:
[scanline 0][scanline 1]...    [scanline 0][scanline 1]...

Each scanline (32 pixels = 4 bytes):
Byte 0: pixels 0-7    [P7 P6 P5 P4 P3 P2 P1 P0]
Byte 1: pixels 8-15   [P15 P14 P13 P12 P11 P10 P9 P8]
Byte 2: pixels 16-23  [P23 P22 P21 P20 P19 P18 P17 P16]
Byte 3: pixels 24-31  [P31 P30 P29 P28 P27 P26 P25 P24]

Color index for pixel (x,y):
bit 0: from bitplane 0, byte (y*4 + x/8), bit (7 - x%8)
bit 1: from bitplane 1, byte (y*4 + x/8), bit (7 - x%8)
```

#### 3.2.3 DMA Display Fetching

The Amiga's custom chips use DMA to fetch display data:

1. **Horizontal Timing**: During each scanline, the display DMA fetches words from each active bitplane
2. **Vertical Timing**: The video beam triggers sequential fetches through memory
3. **Bandwidth Allocation**: Display DMA has priority, potentially stealing cycles from CPU/blitter
4. **Fetch Modes**: Different resolutions require different fetch rates (1x, 2x, 4x)

### 3.3 Color Encoding Systems

#### 3.3.1 Standard Palette Mode

The most common mode uses a fixed palette:

```
Bitplanes | Colors | Color Bits | Usage
----------|--------|------------|------------------
1         | 2      | 1-bit      | Monochrome
2         | 4      | 2-bit      | Simple graphics
3         | 8      | 3-bit      | Basic games
4         | 16     | 4-bit      | Common games
5         | 32     | 5-bit      | Rich graphics
6         | 64     | 6-bit      | EHB or HAM6
7         | 128    | 7-bit      | AGA only
8         | 256    | 8-bit      | AGA only
```

#### 3.3.2 Extra-Halfbrite (EHB) Theory

EHB leverages a hardware trick to double available colors:

```
Bit 5 = 0: Use colors 0-31 normally
Bit 5 = 1: Use colors 0-31 at 50% brightness

Color calculation:
if (index < 32) {
    color = palette[index]
} else {
    base = palette[index - 32]
    color.r = base.r >> 1
    color.g = base.g >> 1
    color.b = base.b >> 1
}
```

#### 3.3.3 Hold-And-Modify (HAM) Theory

HAM exploits spatial coherence in natural images:

**HAM6 Encoding (6 bitplanes):**
```
Bits 5-4: Control code
Bits 3-0: Data value

00: Load from 16-color palette[data]
01: Copy R,G from left; set B = data
10: Copy G,B from left; set R = data
11: Copy R,B from left; set G = data
```

**HAM8 Encoding (8 bitplanes):**
```
Bits 7-6: Control code
Bits 5-0: Data value

00: Load from 64-color palette[data]
01: Copy R,G from left; set B = data
10: Copy G,B from left; set R = data
11: Copy R,B from left; set G = data
```

HAM works because adjacent pixels in photographs tend to have similar colors, so modifying one component while holding others constant is often sufficient.

### 3.4 Resolution and Timing

#### 3.4.1 Horizontal Resolution Modes

The Amiga's horizontal resolution is tied to color clock timing:

```
Mode       | Color Clocks | Pixels | Aspect | Max Colors
-----------|--------------|--------|--------|------------
LoRes      | 1x           | 320    | 2:1    | 32 (64 EHB)
HiRes      | 2x           | 640    | 1:1    | 16
SuperHiRes | 4x           | 1280   | 1:2    | 4
```

Each doubling of resolution halves the available DMA bandwidth for fetching bitplane data, reducing maximum color depth.

#### 3.4.2 Display Timing Constraints

```
NTSC Timing (60Hz):
- Horizontal: 227.5 color clocks per line
- Vertical: 262.5 lines per frame
- Active display: ~200 lines

PAL Timing (50Hz):
- Horizontal: 227 color clocks per line
- Vertical: 312.5 lines per frame
- Active display: ~256 lines
```

#### 3.4.3 Bandwidth Calculations

Display bandwidth requirements:

```
Bandwidth = (width/16) * 2 * bitplanes * height * refresh_rate

Example (320x256, 5 bitplanes, 50Hz PAL):
= (320/16) * 2 * 5 * 256 * 50
= 20 * 2 * 5 * 256 * 50
= 2,560,000 bytes/second
= ~2.44 MB/s
```

### 3.5 Memory Alignment and Padding

#### 3.5.1 Word Alignment Requirements

The Amiga hardware requires strict alignment:

1. **Bitplane Start**: Must be word-aligned (even address)
2. **Scanline Width**: Must be multiple of 16 pixels (word boundary)
3. **Modulo Register**: Compensates for overscan/scrolling

#### 3.5.2 Padding Calculations

```cpp
// Actual memory pitch calculation
uint32_t calculate_pitch(uint32_t width_pixels) {
    // Round up to next multiple of 16 pixels
    uint32_t aligned_pixels = (width_pixels + 15) & ~15;
    // Convert to bytes (16 pixels = 2 bytes)
    return aligned_pixels / 8;
}

// Example: 320 pixels
// Aligned: 320 (already multiple of 16)
// Pitch: 320/8 = 40 bytes

// Example: 318 pixels
// Aligned: 320 (rounded up)
// Pitch: 320/8 = 40 bytes
```

### 3.6 Copper and Display Tricks (Out of Scope)

While not handled by this library, understanding these concepts provides context:

#### 3.6.1 Copper Programming

The Copper allows mid-screen changes:
- Palette changes (color cycling, gradients)
- Resolution switches (mixed mode displays)
- Bitplane pointer updates (infinite scrolling)
- Sprite repositioning

#### 3.6.2 Dual Playfield Mode

Two independent screens with transparency:
- Odd bitplanes (1,3,5): Playfield 1
- Even bitplanes (2,4,6): Playfield 2
- Color 0 transparent in foreground playfield

#### 3.6.3 Hardware Scrolling

Smooth pixel-perfect scrolling via registers:
- Horizontal: BPLCON1 (0-15 pixel delay)
- Vertical: Bitplane pointer manipulation
- Modulo: Skip bytes for virtual viewport

### 3.7 Modern Conversion Challenges

#### 3.7.1 Aspect Ratio Correction

Amiga pixels were designed for CRT displays with non-square pixels:

```
Original Pixel Aspects (relative to square):
- LoRes NTSC: 1.37:1 (wide)
- LoRes PAL: 1.62:1 (wide)
- HiRes NTSC: 0.68:1 (tall)
- HiRes PAL: 0.81:1 (tall)

Practical Correction:
- LoRes: Double width (2:1 approximation)
- HiRes: No correction (close to 1:1)
- Interlace: Double height
```

#### 3.7.2 Color Space Conversion

Amiga colors need expansion from native bit depth:

```cpp
// OCS/ECS: 4-bit to 8-bit expansion
uint8_t expand_4bit_to_8bit(uint8_t value4) {
    // Replicate nibble: 0xA becomes 0xAA
    return (value4 << 4) | value4;
}

// AGA: 8-bit colors (no conversion needed)
uint8_t aga_to_8bit(uint8_t value8) {
    return value8;
}
```

#### 3.7.3 Performance Considerations

Modern conversion bottlenecks:

1. **Bit Extraction**: Expensive bit manipulation for planar→chunky
2. **Memory Access**: Non-contiguous access patterns
3. **HAM Dependency**: Sequential pixel dependencies prevent parallelization
4. **Cache Misses**: Planar format spreads pixel data across memory

Optimization strategies:
- Process full scanlines to improve cache locality
- Use lookup tables for bit extraction
- SIMD for parallel bit operations
- Separate fast paths for common modes

## 4. Architecture Overview

### 2.1 Design Principles

- **Zero-copy where possible**: Use spans and views to avoid unnecessary memory allocation
- **Compile-time optimization**: Leverage C++17 features like `constexpr` and templates
- **RAII**: Automatic resource management through smart pointers
- **Header-only**: Simple integration without complex build systems
- **Cache-friendly**: Optimize memory access patterns for modern CPUs

### 2.2 Namespace Structure

```cpp
namespace amiga {
    namespace video {
        // Core conversion functionality
        namespace detail {
            // Implementation details
        }
    }
}
```

## 3. Core Data Structures

### 3.1 Basic Types

```cpp
namespace amiga::video {
    using u8 = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;

    // RGBA color representation
    struct Color {
        u8 r, g, b, a;

        constexpr Color(u8 r = 0, u8 g = 0, u8 b = 0, u8 a = 255)
            : r(r), g(g), b(b), a(a) {}
    };

    // 12-bit Amiga color (4 bits per channel)
    struct AmigaColor {
        u8 r : 4;
        u8 g : 4;
        u8 b : 4;
        u8 padding : 4;

        constexpr Color to_rgba() const noexcept {
            // Expand 4-bit to 8-bit by replicating nibbles
            return Color{
                static_cast<u8>((r << 4) | r),
                static_cast<u8>((g << 4) | g),
                static_cast<u8>((b << 4) | b)
            };
        }
    };
}
```

### 3.2 Video Mode Flags

```cpp
namespace amiga::video {
    enum class VideoMode : u32 {
        None = 0x0000,
        Lace = 0x0004,           // Interlaced
        EHB = 0x0080,            // Extra-Halfbrite
        HAM = 0x0800,            // Hold-And-Modify
        HiRes = 0x8000,          // High Resolution
        SuperHiRes = 0x8020      // Super High Resolution (AGA)
    };

    // Enable bitwise operations
    constexpr VideoMode operator|(VideoMode a, VideoMode b) {
        return static_cast<VideoMode>(
            static_cast<u32>(a) | static_cast<u32>(b)
        );
    }

    constexpr bool has_mode(VideoMode mode, VideoMode flag) {
        return (static_cast<u32>(mode) & static_cast<u32>(flag)) != 0;
    }
}
```

### 3.3 Screen Descriptor

```cpp
namespace amiga::video {
    struct ScreenInfo {
        u16 width;
        u16 height;
        u8 depth;               // Number of bitplanes (1-8)
        VideoMode mode;

        constexpr u32 bitplane_size() const noexcept {
            // Each scanline must be word-aligned (16 pixels)
            u32 pitch = ((width + 15) / 16) * 2;
            return pitch * height;
        }

        constexpr u32 total_planar_size() const noexcept {
            return bitplane_size() * depth;
        }

        constexpr bool is_ham6() const noexcept {
            return has_mode(mode, VideoMode::HAM) && depth == 6;
        }

        constexpr bool is_ham8() const noexcept {
            return has_mode(mode, VideoMode::HAM) && depth == 8;
        }

        constexpr bool is_ehb() const noexcept {
            return has_mode(mode, VideoMode::EHB) && depth == 6;
        }
    };
}
```

### 3.4 Palette Management

```cpp
namespace amiga::video {
    class Palette {
    private:
        std::vector<Color> colors_;

    public:
        explicit Palette(std::span<const AmigaColor> amiga_colors,
                        VideoMode mode = VideoMode::None) {
            const size_t base_count = amiga_colors.size();

            if (has_mode(mode, VideoMode::EHB)) {
                // EHB: Generate half-bright colors
                colors_.reserve(base_count * 2);
                for (auto& ac : amiga_colors) {
                    colors_.push_back(ac.to_rgba());
                }
                for (size_t i = 0; i < base_count; ++i) {
                    Color half = colors_[i];
                    half.r >>= 1;
                    half.g >>= 1;
                    half.b >>= 1;
                    colors_.push_back(half);
                }
            } else {
                colors_.reserve(base_count);
                for (auto& ac : amiga_colors) {
                    colors_.push_back(ac.to_rgba());
                }
            }
        }

        const Color& operator[](size_t index) const {
            return colors_[index];
        }

        size_t size() const noexcept { return colors_.size(); }
    };
}
```

## 4. Conversion Algorithms

### 4.1 Bitplane to Chunky Conversion

```cpp
namespace amiga::video {
    // Convert planar data to chunky indexed format
    void bitplanes_to_chunky(
        std::span<const u8*> bitplanes,  // Array of bitplane pointers
        std::span<u8> output,             // Output chunky buffer
        const ScreenInfo& info
    ) {
        const u32 width = info.width;
        const u32 height = info.height;
        const u8 depth = info.depth;
        const u32 pitch = ((width + 15) / 16) * 2;

        for (u32 y = 0; y < height; ++y) {
            for (u32 x = 0; x < width; ++x) {
                u8 pixel_index = 0;

                // Calculate byte and bit position
                const u32 byte_offset = (y * pitch) + (x >> 3);
                const u8 bit_mask = 0x80 >> (x & 7);

                // Gather bits from each plane
                for (u8 plane = 0; plane < depth; ++plane) {
                    if (bitplanes[plane][byte_offset] & bit_mask) {
                        pixel_index |= (1 << plane);
                    }
                }

                output[y * width + x] = pixel_index;
            }
        }
    }
}
```

### 4.2 Chunky to RGB Conversion

```cpp
namespace amiga::video {
    void chunky_to_rgb(
        std::span<const u8> chunky,
        std::span<Color> output,
        const Palette& palette
    ) {
        std::transform(chunky.begin(), chunky.end(), output.begin(),
            [&palette](u8 index) { return palette[index]; });
    }
}
```

### 4.3 HAM (Hold-And-Modify) Conversion

```cpp
namespace amiga::video::detail {
    enum class HAMOp : u8 {
        SetPalette = 0b00,
        ModifyBlue = 0b01,
        ModifyRed = 0b10,
        ModifyGreen = 0b11
    };

    inline Color process_ham6_pixel(u8 ham_value, Color previous,
                                    const Palette& palette) {
        const HAMOp op = static_cast<HAMOp>(ham_value >> 4);
        const u8 data = ham_value & 0x0F;

        Color result = previous;

        switch (op) {
            case HAMOp::SetPalette:
                result = palette[data];
                break;
            case HAMOp::ModifyBlue:
                result.b = (data << 4) | data;
                break;
            case HAMOp::ModifyRed:
                result.r = (data << 4) | data;
                break;
            case HAMOp::ModifyGreen:
                result.g = (data << 4) | data;
                break;
        }

        return result;
    }

    inline Color process_ham8_pixel(u8 ham_value, Color previous,
                                    const Palette& palette) {
        const HAMOp op = static_cast<HAMOp>(ham_value >> 6);
        const u8 data = ham_value & 0x3F;

        Color result = previous;

        switch (op) {
            case HAMOp::SetPalette:
                result = palette[data];
                break;
            case HAMOp::ModifyBlue:
                result.b = data << 2;
                break;
            case HAMOp::ModifyRed:
                result.r = data << 2;
                break;
            case HAMOp::ModifyGreen:
                result.g = data << 2;
                break;
        }

        return result;
    }
}

namespace amiga::video {
    void ham_to_rgb(
        std::span<const u8*> bitplanes,
        std::span<Color> output,
        const ScreenInfo& info,
        const Palette& palette
    ) {
        // First convert to chunky
        std::vector<u8> chunky(info.width * info.height);
        bitplanes_to_chunky(bitplanes, chunky, info);

        // Process HAM data
        Color previous = palette[0];  // Background color

        for (size_t y = 0; y < info.height; ++y) {
            // Reset to background at start of each line
            previous = palette[0];

            for (size_t x = 0; x < info.width; ++x) {
                const size_t idx = y * info.width + x;
                const u8 ham_value = chunky[idx];

                if (info.is_ham6()) {
                    previous = detail::process_ham6_pixel(
                        ham_value, previous, palette);
                } else {  // HAM8
                    previous = detail::process_ham8_pixel(
                        ham_value, previous, palette);
                }

                output[idx] = previous;
            }
        }
    }
}
```

### 4.4 Direct Bitplane to RGB Conversion

```cpp
namespace amiga::video {
    void bitplanes_to_rgb(
        std::span<const u8*> bitplanes,
        std::span<Color> output,
        const ScreenInfo& info,
        const Palette& palette
    ) {
        if (info.is_ham6() || info.is_ham8()) {
            ham_to_rgb(bitplanes, output, info, palette);
        } else {
            // Standard conversion via chunky
            std::vector<u8> chunky(info.width * info.height);
            bitplanes_to_chunky(bitplanes, chunky, info);
            chunky_to_rgb(chunky, output, palette);
        }
    }
}
```

### 4.5 Aspect Ratio Correction

```cpp
namespace amiga::video {
    struct CorrectedDimensions {
        u32 width;
        u32 height;

        constexpr CorrectedDimensions(const ScreenInfo& info) noexcept
            : width(info.width), height(info.height) {

            // Horizontal scaling based on resolution
            if (!has_mode(info.mode, VideoMode::HiRes) &&
                !has_mode(info.mode, VideoMode::SuperHiRes)) {
                // LoRes: double width
                width *= 2;
            }
            // HiRes: no change (1:1 pixels)
            // SuperHiRes: no change (treated as 1:1)

            // Vertical scaling for interlace
            if (has_mode(info.mode, VideoMode::Lace)) {
                height *= 2;
            }
        }
    };

    template<typename T>
    void apply_aspect_correction(
        std::span<const T> input,
        std::span<T> output,
        const ScreenInfo& info
    ) {
        const CorrectedDimensions dims(info);
        const u32 src_width = info.width;
        const u32 src_height = info.height;

        const bool double_width = (dims.width == src_width * 2);
        const bool double_height = (dims.height == src_height * 2);

        for (u32 sy = 0; sy < src_height; ++sy) {
            const u32 dy_start = double_height ? sy * 2 : sy;
            const u32 dy_end = double_height ? dy_start + 2 : dy_start + 1;

            for (u32 dy = dy_start; dy < dy_end; ++dy) {
                for (u32 sx = 0; sx < src_width; ++sx) {
                    const T pixel = input[sy * src_width + sx];
                    const u32 dx_start = double_width ? sx * 2 : sx;
                    const u32 dx_end = double_width ? dx_start + 2 : dx_start + 1;

                    for (u32 dx = dx_start; dx < dx_end; ++dx) {
                        output[dy * dims.width + dx] = pixel;
                    }
                }
            }
        }
    }
}
```

## 5. Memory Management

### 5.1 Bitplane Buffer Management

```cpp
namespace amiga::video {
    class BitplaneBuffer {
    private:
        std::vector<u8> data_;
        std::vector<const u8*> plane_ptrs_;
        ScreenInfo info_;

    public:
        BitplaneBuffer(const ScreenInfo& info)
            : info_(info),
              data_(info.total_planar_size()),
              plane_ptrs_(info.depth) {

            const size_t plane_size = info.bitplane_size();
            for (u8 i = 0; i < info.depth; ++i) {
                plane_ptrs_[i] = data_.data() + (i * plane_size);
            }
        }

        // Load non-interleaved data
        void load_planar(std::span<const u8> source) {
            std::copy(source.begin(), source.end(), data_.begin());
        }

        // Deinterleave ILBM-style data
        void load_interleaved(std::span<const u8> source) {
            const u32 pitch = ((info_.width + 15) / 16) * 2;
            const u32 plane_size = info_.bitplane_size();

            for (u32 y = 0; y < info_.height; ++y) {
                for (u8 p = 0; p < info_.depth; ++p) {
                    const u32 src_offset = (y * info_.depth + p) * pitch;
                    const u32 dst_offset = p * plane_size + y * pitch;

                    std::copy_n(&source[src_offset], pitch,
                               &data_[dst_offset]);
                }
            }
        }

        std::span<const u8*> planes() const {
            return plane_ptrs_;
        }
    };
}
```

## 6. High-Level API

### 6.1 Converter Class

```cpp
namespace amiga::video {
    class Converter {
    private:
        ScreenInfo info_;
        Palette palette_;
        BitplaneBuffer buffer_;

    public:
        Converter(const ScreenInfo& info,
                 std::span<const AmigaColor> colors)
            : info_(info),
              palette_(colors, info.mode),
              buffer_(info) {}

        // Load data methods
        void load_planar_data(std::span<const u8> data) {
            buffer_.load_planar(data);
        }

        void load_interleaved_data(std::span<const u8> data) {
            buffer_.load_interleaved(data);
        }

        // Conversion methods
        std::vector<u8> to_chunky() const {
            std::vector<u8> result(info_.width * info_.height);
            bitplanes_to_chunky(buffer_.planes(), result, info_);
            return result;
        }

        std::vector<Color> to_rgb() const {
            std::vector<Color> result(info_.width * info_.height);
            bitplanes_to_rgb(buffer_.planes(), result, info_, palette_);
            return result;
        }

        std::vector<Color> to_corrected_rgb() const {
            auto uncorrected = to_rgb();
            CorrectedDimensions dims(info_);
            std::vector<Color> result(dims.width * dims.height);
            apply_aspect_correction<Color>(uncorrected, result, info_);
            return result;
        }

        // Get dimensions
        CorrectedDimensions corrected_dimensions() const {
            return CorrectedDimensions(info_);
        }
    };
}
```

## 7. Usage Examples

### 7.1 Basic Conversion

```cpp
#include "amiga_video.hpp"

// Load a 320x256 32-color screen
amiga::video::ScreenInfo info{
    320, 256, 5,
    amiga::video::VideoMode::None
};

// Define palette (32 colors)
std::array<amiga::video::AmigaColor, 32> amiga_palette = {
    // ... color definitions
};

// Create converter
amiga::video::Converter converter(info, amiga_palette);

// Load bitplane data
std::vector<uint8_t> planar_data = load_file("screen.raw");
converter.load_planar_data(planar_data);

// Convert to RGB with aspect correction
auto rgb_data = converter.to_corrected_rgb();
auto dims = converter.corrected_dimensions();

// rgb_data now contains dims.width * dims.height RGBA pixels
```

### 7.2 HAM6 Conversion

```cpp
// HAM6 screen
amiga::video::ScreenInfo ham_info{
    320, 256, 6,
    amiga::video::VideoMode::HAM | amiga::video::VideoMode::Lace
};

// Base 16-color palette for HAM
std::array<amiga::video::AmigaColor, 16> ham_palette = {
    // ... base colors
};

amiga::video::Converter ham_converter(ham_info, ham_palette);
ham_converter.load_interleaved_data(ilbm_data);

// Direct to RGB (required for HAM)
auto ham_rgb = ham_converter.to_corrected_rgb();
```

## 8. Performance Considerations

### 8.1 Optimization Strategies

1. **Bitplane Access Pattern**: Process data scanline by scanline to maximize cache locality
2. **SIMD Potential**: The bitplane-to-chunky conversion can benefit from SIMD instructions for parallel bit extraction
3. **Memory Allocation**: Pre-allocate buffers to avoid dynamic allocation during conversion
4. **Template Specialization**: Use compile-time specialization for known video modes

### 8.2 Parallel Processing

```cpp
namespace amiga::video {
    // Parallel version using std::execution
    template<typename ExecutionPolicy>
    void bitplanes_to_chunky_parallel(
        ExecutionPolicy&& policy,
        std::span<const u8*> bitplanes,
        std::span<u8> output,
        const ScreenInfo& info
    ) {
        const u32 width = info.width;
        const u32 height = info.height;

        std::for_each(policy,
            counting_iterator(0u), counting_iterator(height),
            [=](u32 y) {
                // Process each scanline independently
                const u32 pitch = ((width + 15) / 16) * 2;

                for (u32 x = 0; x < width; ++x) {
                    u8 pixel_index = 0;
                    const u32 byte_offset = (y * pitch) + (x >> 3);
                    const u8 bit_mask = 0x80 >> (x & 7);

                    for (u8 plane = 0; plane < info.depth; ++plane) {
                        if (bitplanes[plane][byte_offset] & bit_mask) {
                            pixel_index |= (1 << plane);
                        }
                    }

                    output[y * width + x] = pixel_index;
                }
            });
    }
}
```

## 9. Testing Strategy

### 9.1 Unit Tests

- **Bitplane conversion**: Verify correct bit extraction and index assembly
- **Palette expansion**: Test 4-to-8 bit conversion and EHB generation
- **HAM decoding**: Validate hold-and-modify operations
- **Aspect ratio**: Confirm correct scaling factors for each mode

### 9.2 Reference Images

Test against known good conversions of standard Amiga images:
- Standard 32-color screens (OCS/ECS)
- HAM6 images
- EHB mode screens
- AGA 256-color screens
- Interlaced displays

## 10. Build Configuration

### 10.1 CMake Example

```cmake
cmake_minimum_required(VERSION 3.12)
project(amiga_video_converter VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header-only library
add_library(amiga_video INTERFACE)
target_include_directories(amiga_video INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Optional: Enable parallel STL
find_package(TBB)
if(TBB_FOUND)
    target_link_libraries(amiga_video INTERFACE TBB::tbb)
    target_compile_definitions(amiga_video INTERFACE
        AMIGA_VIDEO_PARALLEL_AVAILABLE)
endif()

# Testing
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
```

## 11. Future Enhancements

### 11.1 Potential Extensions

1. **Copper list simulation**: Basic support for mid-screen palette changes
2. **Sprite overlay**: Composite hardware sprites onto the display
3. **Output formats**: Direct support for common image formats (PNG, BMP)
4. **Real-time conversion**: Optimize for emulator integration
5. **AGA enhancements**: Full 24-bit color support, fetch modes

### 11.2 Platform-Specific Optimizations

- **x86-64**: AVX2 instructions for parallel bit manipulation
- **ARM**: NEON intrinsics for mobile/embedded platforms
- **GPU acceleration**: Compute shaders for bulk conversion

## 12. Conclusion

This design provides a solid foundation for accurate and efficient Amiga video format conversion. The C++17 implementation leverages modern language features while maintaining simplicity and performance. The modular architecture allows for easy extension and optimization as needed.
