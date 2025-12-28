#ifndef ONYX_IMAGE_PALETTES_HPP_
#define ONYX_IMAGE_PALETTES_HPP_

#include <onyx_image/onyx_image_export.h>

#include <array>
#include <cstdint>
#include <span>

namespace onyx_image {

// ============================================================================
// Standard Retro Computer Palettes
// ============================================================================
//
// All palettes are returned as RGB888 triplets (3 bytes per color).
// Values are derived from hardware specifications and measurements.

// ----------------------------------------------------------------------------
// IBM CGA (Color Graphics Adapter) - 16 colors
// ----------------------------------------------------------------------------
// RGBI encoding: 4-bit color (1 bit each for R, G, B, plus Intensity)
// The "brown" color (index 6) is a hardware quirk where dark yellow
// appears as brown due to modified green signal.

[[nodiscard]] constexpr std::array<std::uint8_t, 16 * 3> cga_palette() noexcept {
    return {{
        0x00, 0x00, 0x00,  //  0: Black
        0x00, 0x00, 0xAA,  //  1: Blue
        0x00, 0xAA, 0x00,  //  2: Green
        0x00, 0xAA, 0xAA,  //  3: Cyan
        0xAA, 0x00, 0x00,  //  4: Red
        0xAA, 0x00, 0xAA,  //  5: Magenta
        0xAA, 0x55, 0x00,  //  6: Brown (dark yellow with reduced green)
        0xAA, 0xAA, 0xAA,  //  7: Light Gray
        0x55, 0x55, 0x55,  //  8: Dark Gray
        0x55, 0x55, 0xFF,  //  9: Light Blue
        0x55, 0xFF, 0x55,  // 10: Light Green
        0x55, 0xFF, 0xFF,  // 11: Light Cyan
        0xFF, 0x55, 0x55,  // 12: Light Red
        0xFF, 0x55, 0xFF,  // 13: Light Magenta
        0xFF, 0xFF, 0x55,  // 14: Yellow
        0xFF, 0xFF, 0xFF   // 15: White
    }};
}

// CGA 4-color palettes for 320x200 mode
// Palette 0: Black, Green, Red, Brown/Yellow
// Palette 1: Black, Cyan, Magenta, White

[[nodiscard]] constexpr std::array<std::uint8_t, 4 * 3> cga_palette0_low() noexcept {
    return {{
        0x00, 0x00, 0x00,  // 0: Black
        0x00, 0xAA, 0x00,  // 1: Green
        0xAA, 0x00, 0x00,  // 2: Red
        0xAA, 0x55, 0x00   // 3: Brown
    }};
}

[[nodiscard]] constexpr std::array<std::uint8_t, 4 * 3> cga_palette0_high() noexcept {
    return {{
        0x00, 0x00, 0x00,  // 0: Black
        0x55, 0xFF, 0x55,  // 1: Light Green
        0xFF, 0x55, 0x55,  // 2: Light Red
        0xFF, 0xFF, 0x55   // 3: Yellow
    }};
}

[[nodiscard]] constexpr std::array<std::uint8_t, 4 * 3> cga_palette1_low() noexcept {
    return {{
        0x00, 0x00, 0x00,  // 0: Black
        0x00, 0xAA, 0xAA,  // 1: Cyan
        0xAA, 0x00, 0xAA,  // 2: Magenta
        0xAA, 0xAA, 0xAA   // 3: Light Gray
    }};
}

[[nodiscard]] constexpr std::array<std::uint8_t, 4 * 3> cga_palette1_high() noexcept {
    return {{
        0x00, 0x00, 0x00,  // 0: Black
        0x55, 0xFF, 0xFF,  // 1: Light Cyan
        0xFF, 0x55, 0xFF,  // 2: Light Magenta
        0xFF, 0xFF, 0xFF   // 3: White
    }};
}

// ----------------------------------------------------------------------------
// IBM EGA (Enhanced Graphics Adapter) - 16 colors from 64
// ----------------------------------------------------------------------------
// 6-bit palette (2 bits per channel: 0, 1, 2, 3 -> 0x00, 0x55, 0xAA, 0xFF)
// Default 16-color palette matches CGA for compatibility.

[[nodiscard]] constexpr std::array<std::uint8_t, 16 * 3> ega_default_palette() noexcept {
    return cga_palette();
}

// Convert 6-bit EGA color (0-63) to RGB888
[[nodiscard]] constexpr std::array<std::uint8_t, 3> ega_color_to_rgb(std::uint8_t color) noexcept {
    // EGA uses 2 bits per channel: RrGgBb
    // r,g,b are low bits, R,G,B are high bits
    // Component value: 0b00=0x00, 0b01=0x55, 0b10=0xAA, 0b11=0xFF
    constexpr std::uint8_t levels[4] = {0x00, 0x55, 0xAA, 0xFF};

    std::uint8_t r_bits = ((color >> 2) & 0x02) | ((color >> 5) & 0x01);
    std::uint8_t g_bits = ((color >> 1) & 0x02) | ((color >> 4) & 0x01);
    std::uint8_t b_bits = ((color >> 0) & 0x02) | ((color >> 3) & 0x01);

    return {{levels[r_bits], levels[g_bits], levels[b_bits]}};
}

// Full 64-color EGA palette
[[nodiscard]] constexpr std::array<std::uint8_t, 64 * 3> ega_full_palette() noexcept {
    std::array<std::uint8_t, 64 * 3> palette{};
    for (std::size_t i = 0; i < 64; ++i) {
        auto rgb = ega_color_to_rgb(static_cast<std::uint8_t>(i));
        palette[i * 3 + 0] = rgb[0];
        palette[i * 3 + 1] = rgb[1];
        palette[i * 3 + 2] = rgb[2];
    }
    return palette;
}

// ----------------------------------------------------------------------------
// IBM VGA (Video Graphics Array) - 256 colors
// ----------------------------------------------------------------------------
// 18-bit DAC (6 bits per channel), default Mode 13h palette.
// First 16 colors match CGA, colors 16-255 follow standard VGA arrangement.

[[nodiscard]] ONYX_IMAGE_EXPORT
std::array<std::uint8_t, 256 * 3> vga_default_palette() noexcept;

// Convert 6-bit VGA DAC value (0-63) to 8-bit
[[nodiscard]] constexpr std::uint8_t vga_6bit_to_8bit(std::uint8_t value) noexcept {
    // Scale 0-63 to 0-255: (value * 255) / 63, or approximate: (value << 2) | (value >> 4)
    return static_cast<std::uint8_t>((value << 2) | (value >> 4));
}

// ----------------------------------------------------------------------------
// Commodore 64 (VIC-II) - 16 fixed colors
// ----------------------------------------------------------------------------
// Values from VIC-II chip analysis by Philip "Pepto" Timmermann.
// These are the widely-accepted "Pepto" palette values.

[[nodiscard]] constexpr std::array<std::uint8_t, 16 * 3> c64_palette() noexcept {
    return {{
        0x00, 0x00, 0x00,  //  0: Black
        0xFF, 0xFF, 0xFF,  //  1: White
        0x68, 0x37, 0x2B,  //  2: Red
        0x70, 0xA4, 0xB2,  //  3: Cyan
        0x6F, 0x3D, 0x86,  //  4: Purple
        0x58, 0x8D, 0x43,  //  5: Green
        0x35, 0x28, 0x79,  //  6: Blue
        0xB8, 0xC7, 0x6F,  //  7: Yellow
        0x6F, 0x4F, 0x25,  //  8: Orange
        0x43, 0x39, 0x00,  //  9: Brown
        0x9A, 0x67, 0x59,  // 10: Light Red
        0x44, 0x44, 0x44,  // 11: Dark Gray
        0x6C, 0x6C, 0x6C,  // 12: Medium Gray
        0x9A, 0xD2, 0x84,  // 13: Light Green
        0x6C, 0x5E, 0xB5,  // 14: Light Blue
        0x95, 0x95, 0x95   // 15: Light Gray
    }};
}

// Alternative C64 palette: "Colodore" by Pepto (revised 2017)
[[nodiscard]] constexpr std::array<std::uint8_t, 16 * 3> c64_colodore_palette() noexcept {
    return {{
        0x00, 0x00, 0x00,  //  0: Black
        0xFF, 0xFF, 0xFF,  //  1: White
        0x81, 0x33, 0x38,  //  2: Red
        0x75, 0xCE, 0xC8,  //  3: Cyan
        0x8E, 0x3C, 0x97,  //  4: Purple
        0x56, 0xAC, 0x4D,  //  5: Green
        0x2E, 0x2C, 0x9B,  //  6: Blue
        0xED, 0xF1, 0x71,  //  7: Yellow
        0x8E, 0x50, 0x29,  //  8: Orange
        0x55, 0x38, 0x00,  //  9: Brown
        0xC4, 0x6C, 0x71,  // 10: Light Red
        0x4A, 0x4A, 0x4A,  // 11: Dark Gray
        0x7B, 0x7B, 0x7B,  // 12: Medium Gray
        0xA9, 0xFF, 0x9F,  // 13: Light Green
        0x70, 0x6D, 0xEB,  // 14: Light Blue
        0xB2, 0xB2, 0xB2   // 15: Light Gray
    }};
}

// ----------------------------------------------------------------------------
// Commodore Amiga (OCS/ECS) - 12-bit color (4096 colors)
// ----------------------------------------------------------------------------
// Amiga uses 4 bits per channel. No fixed palette, but common defaults exist.

// Convert 12-bit Amiga color (0xRGB) to RGB888
[[nodiscard]] constexpr std::array<std::uint8_t, 3> amiga_color_to_rgb(std::uint16_t color) noexcept {
    // 12-bit: 0x0RGB, scale each 4-bit nibble to 8-bit
    std::uint8_t r = static_cast<std::uint8_t>((color >> 8) & 0x0F);
    std::uint8_t g = static_cast<std::uint8_t>((color >> 4) & 0x0F);
    std::uint8_t b = static_cast<std::uint8_t>(color & 0x0F);

    // Scale 0-15 to 0-255: (n << 4) | n
    return {{
        static_cast<std::uint8_t>((r << 4) | r),
        static_cast<std::uint8_t>((g << 4) | g),
        static_cast<std::uint8_t>((b << 4) | b)
    }};
}

// Amiga Workbench 1.x default 4-color palette
[[nodiscard]] constexpr std::array<std::uint8_t, 4 * 3> amiga_wb1_palette() noexcept {
    return {{
        0x00, 0x55, 0xAA,  // 0: Blue (background)
        0xFF, 0xFF, 0xFF,  // 1: White
        0x00, 0x00, 0x00,  // 2: Black
        0xFF, 0x88, 0x00   // 3: Orange
    }};
}

// Amiga Workbench 2.x default 4-color palette
[[nodiscard]] constexpr std::array<std::uint8_t, 4 * 3> amiga_wb2_palette() noexcept {
    return {{
        0x95, 0x95, 0x95,  // 0: Gray (background)
        0x00, 0x00, 0x00,  // 1: Black
        0xFF, 0xFF, 0xFF,  // 2: White
        0x3B, 0x67, 0xA2   // 3: Blue
    }};
}

// Amiga Workbench 3.x default 8-color palette (MagicWB style)
[[nodiscard]] constexpr std::array<std::uint8_t, 8 * 3> amiga_wb3_palette() noexcept {
    return {{
        0x95, 0x95, 0x95,  // 0: Gray
        0x00, 0x00, 0x00,  // 1: Black
        0xFF, 0xFF, 0xFF,  // 2: White
        0x3B, 0x67, 0xA2,  // 3: Blue
        0x7B, 0x7B, 0x7B,  // 4: Dark Gray
        0xAF, 0xAF, 0xAF,  // 5: Light Gray
        0xAA, 0x90, 0x7C,  // 6: Beige
        0xFF, 0xA9, 0x97   // 7: Salmon
    }};
}

// Common Amiga 32-color palette (based on DPaint defaults)
[[nodiscard]] ONYX_IMAGE_EXPORT
std::array<std::uint8_t, 32 * 3> amiga_dpaint_palette() noexcept;

// ----------------------------------------------------------------------------
// Atari ST - 9-bit color (512 colors)
// ----------------------------------------------------------------------------
// ST uses 3 bits per channel. Common palettes for Low/Med/High res modes.

// Convert 9-bit Atari ST color (0x0RGB, 3 bits each) to RGB888
[[nodiscard]] constexpr std::array<std::uint8_t, 3> atarist_color_to_rgb(std::uint16_t color) noexcept {
    // 9-bit: 0x0RGB (3 bits per channel in low nibbles)
    // Hardware quirk: bits are stored as 0x0rgb where each digit is 0-7
    std::uint8_t r = static_cast<std::uint8_t>((color >> 8) & 0x07);
    std::uint8_t g = static_cast<std::uint8_t>((color >> 4) & 0x07);
    std::uint8_t b = static_cast<std::uint8_t>(color & 0x07);

    // Scale 0-7 to 0-255: multiply by 36.43 (approximately (n * 255) / 7)
    // Or use: (n << 5) | (n << 2) | (n >> 1) for better precision
    auto scale = [](std::uint8_t v) -> std::uint8_t {
        return static_cast<std::uint8_t>((v << 5) | (v << 2) | (v >> 1));
    };

    return {{scale(r), scale(g), scale(b)}};
}

// Atari ST default low-res 16-color palette
[[nodiscard]] constexpr std::array<std::uint8_t, 16 * 3> atarist_default_palette() noexcept {
    return {{
        0xFF, 0xFF, 0xFF,  //  0: White
        0xFF, 0x00, 0x00,  //  1: Red
        0x00, 0xFF, 0x00,  //  2: Green
        0xFF, 0xFF, 0x00,  //  3: Yellow
        0x00, 0x00, 0xFF,  //  4: Blue
        0xFF, 0x00, 0xFF,  //  5: Magenta
        0x00, 0xFF, 0xFF,  //  6: Cyan
        0xB6, 0xB6, 0xB6,  //  7: Light Gray
        0x49, 0x49, 0x49,  //  8: Dark Gray
        0x92, 0x00, 0x00,  //  9: Dark Red
        0x00, 0x92, 0x00,  // 10: Dark Green
        0x92, 0x92, 0x00,  // 11: Dark Yellow
        0x00, 0x00, 0x92,  // 12: Dark Blue
        0x92, 0x00, 0x92,  // 13: Dark Magenta
        0x00, 0x92, 0x92,  // 14: Dark Cyan
        0x00, 0x00, 0x00   // 15: Black
    }};
}

// ----------------------------------------------------------------------------
// Grayscale palettes
// ----------------------------------------------------------------------------

// Generate n-level grayscale palette
template <std::size_t N>
[[nodiscard]] constexpr std::array<std::uint8_t, N * 3> grayscale_palette() noexcept {
    std::array<std::uint8_t, N * 3> palette{};
    for (std::size_t i = 0; i < N; ++i) {
        std::uint8_t gray = static_cast<std::uint8_t>((i * 255) / (N - 1));
        palette[i * 3 + 0] = gray;
        palette[i * 3 + 1] = gray;
        palette[i * 3 + 2] = gray;
    }
    return palette;
}

// Common grayscale palettes
[[nodiscard]] constexpr std::array<std::uint8_t, 2 * 3> grayscale_1bit_palette() noexcept {
    return {{0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF}};
}

[[nodiscard]] constexpr std::array<std::uint8_t, 4 * 3> grayscale_2bit_palette() noexcept {
    return grayscale_palette<4>();
}

[[nodiscard]] constexpr std::array<std::uint8_t, 16 * 3> grayscale_4bit_palette() noexcept {
    return grayscale_palette<16>();
}

[[nodiscard]] constexpr std::array<std::uint8_t, 256 * 3> grayscale_8bit_palette() noexcept {
    return grayscale_palette<256>();
}

} // namespace onyx_image

#endif // ONYX_IMAGE_PALETTES_HPP_
