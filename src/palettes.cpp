#include <onyx_image/palettes.hpp>

namespace onyx_image {

// VGA default Mode 13h 256-color palette
// Structure: 0-15 = CGA colors, 16-31 = grayscale, 32-255 = color cube + ramps
std::array<std::uint8_t, 256 * 3> vga_default_palette() noexcept {
    std::array<std::uint8_t, 256 * 3> palette{};

    // Colors 0-15: CGA compatibility colors
    auto cga = cga_palette();
    for (std::size_t i = 0; i < 16 * 3; ++i) {
        palette[i] = cga[i];
    }

    // Colors 16-31: 16-level grayscale
    for (std::size_t i = 0; i < 16; ++i) {
        std::uint8_t gray = vga_6bit_to_8bit(static_cast<std::uint8_t>(i * 63 / 15));
        palette[(16 + i) * 3 + 0] = gray;
        palette[(16 + i) * 3 + 1] = gray;
        palette[(16 + i) * 3 + 2] = gray;
    }

    // Colors 32-255: Color cube (6x6x6 = 216 colors) + color ramps
    // The VGA default palette uses a specific arrangement with color ramps

    // Helper to set palette entry from 6-bit RGB
    auto set_color = [&](std::size_t index, int r6, int g6, int b6) {
        palette[index * 3 + 0] = vga_6bit_to_8bit(static_cast<std::uint8_t>(r6));
        palette[index * 3 + 1] = vga_6bit_to_8bit(static_cast<std::uint8_t>(g6));
        palette[index * 3 + 2] = vga_6bit_to_8bit(static_cast<std::uint8_t>(b6));
    };

    // Colors 32-55: Red ramp with variations
    // Colors 56-79: Green ramp with variations
    // Colors 80-103: Blue ramp with variations
    // Colors 104-127: Yellow/Orange ramp
    // Colors 128-151: Cyan ramp
    // Colors 152-175: Magenta ramp
    // Colors 176-255: Mixed color ramps

    // Standard VGA palette color blocks (based on BIOS defaults)
    // Block 1: 8 intensity levels x 3 saturation levels for each of 8 hues
    std::size_t idx = 32;

    // Generate color ramps for primary and secondary colors
    // Each hue has 24 entries: 8 intensities x 3 saturation levels
    constexpr int hue_count = 8;
    constexpr int sat_levels = 3;
    constexpr int int_levels = 8;

    // Hue definitions (in 6-bit RGB at max intensity/saturation)
    constexpr int hues[hue_count][3] = {
        {63,  0,  0},  // Red
        {63, 31,  0},  // Orange
        {63, 63,  0},  // Yellow
        { 0, 63,  0},  // Green
        { 0, 63, 63},  // Cyan
        { 0,  0, 63},  // Blue
        {31,  0, 63},  // Purple
        {63,  0, 63}   // Magenta
    };

    for (int hue = 0; hue < hue_count; ++hue) {
        for (int sat = 0; sat < sat_levels; ++sat) {
            for (int intensity = 0; intensity < int_levels; ++intensity) {
                if (idx >= 256) break;

                // Calculate color with saturation and intensity
                int r = hues[hue][0];
                int g = hues[hue][1];
                int b = hues[hue][2];

                // Apply intensity (scale down)
                int int_scale = (intensity + 1) * 8;  // 8, 16, 24, 32, 40, 48, 56, 64
                r = (r * int_scale) / 64;
                g = (g * int_scale) / 64;
                b = (b * int_scale) / 64;

                // Apply saturation (blend toward gray)
                if (sat > 0) {
                    int gray = (r + g + b) / 3;
                    int sat_factor = (sat == 1) ? 2 : 4;  // 1/2 or 1/4 saturation
                    r = r + (gray - r) / sat_factor;
                    g = g + (gray - g) / sat_factor;
                    b = b + (gray - b) / sat_factor;
                }

                set_color(idx++, r, g, b);
            }
        }
    }

    // Fill remaining entries with additional grayscale/colors if needed
    while (idx < 256) {
        int gray = static_cast<int>(((idx - 224) * 63) / 31);
        set_color(idx++, gray, gray, gray);
    }

    return palette;
}

// Amiga Deluxe Paint default 32-color palette
std::array<std::uint8_t, 32 * 3> amiga_dpaint_palette() noexcept {
    // Classic DPaint default palette - commonly used starting point
    return {{
        0x00, 0x00, 0x00,  //  0: Black
        0xFF, 0xFF, 0xFF,  //  1: White
        0xFF, 0x00, 0x00,  //  2: Red
        0x00, 0xFF, 0x00,  //  3: Green
        0x00, 0x00, 0xFF,  //  4: Blue
        0xFF, 0xFF, 0x00,  //  5: Yellow
        0xFF, 0x00, 0xFF,  //  6: Magenta
        0x00, 0xFF, 0xFF,  //  7: Cyan
        0xAA, 0x00, 0x00,  //  8: Dark Red
        0x00, 0xAA, 0x00,  //  9: Dark Green
        0x00, 0x00, 0xAA,  // 10: Dark Blue
        0xAA, 0xAA, 0x00,  // 11: Dark Yellow
        0xAA, 0x00, 0xAA,  // 12: Dark Magenta
        0x00, 0xAA, 0xAA,  // 13: Dark Cyan
        0xAA, 0xAA, 0xAA,  // 14: Light Gray
        0x55, 0x55, 0x55,  // 15: Dark Gray
        0xFF, 0xAA, 0xAA,  // 16: Light Red
        0xAA, 0xFF, 0xAA,  // 17: Light Green
        0xAA, 0xAA, 0xFF,  // 18: Light Blue
        0xFF, 0xFF, 0xAA,  // 19: Light Yellow
        0xFF, 0xAA, 0xFF,  // 20: Light Magenta
        0xAA, 0xFF, 0xFF,  // 21: Light Cyan
        0xFF, 0x55, 0x00,  // 22: Orange
        0x00, 0xFF, 0x55,  // 23: Spring Green
        0x55, 0x00, 0xFF,  // 24: Violet
        0xFF, 0x55, 0xAA,  // 25: Pink
        0x55, 0xFF, 0x00,  // 26: Lime
        0x00, 0x55, 0xFF,  // 27: Sky Blue
        0x88, 0x44, 0x00,  // 28: Brown
        0x44, 0x88, 0x44,  // 29: Olive
        0x44, 0x44, 0x88,  // 30: Navy
        0x88, 0x88, 0x88   // 31: Gray
    }};
}

} // namespace onyx_image
