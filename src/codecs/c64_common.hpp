#ifndef ONYX_IMAGE_CODECS_C64_COMMON_HPP_
#define ONYX_IMAGE_CODECS_C64_COMMON_HPP_

#include <cstddef>
#include <onyx_image/surface.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace onyx_image::c64 {

// ============================================================================
// C64 Common Definitions
// ============================================================================

// C64 palette (VICE default, from RECOIL)
constexpr std::array<std::uint32_t, 16> PALETTE = {{
    0x000000,  // 0: Black
    0xffffff,  // 1: White
    0x68372b,  // 2: Red
    0x70a4b2,  // 3: Cyan
    0x6f3d86,  // 4: Purple
    0x588d43,  // 5: Green
    0x352879,  // 6: Blue
    0xb8c76f,  // 7: Yellow
    0x6f4f25,  // 8: Orange
    0x433900,  // 9: Brown
    0x9a6759,  // 10: Light Red
    0x444444,  // 11: Dark Gray
    0x6c6c6c,  // 12: Gray
    0x9ad284,  // 13: Light Green
    0x6c5eb5,  // 14: Light Blue
    0x959595,  // 15: Light Gray
}};

// Common constants
constexpr int MULTICOLOR_WIDTH = 320;
constexpr int MULTICOLOR_HEIGHT = 200;
constexpr int HIRES_WIDTH = 320;
constexpr int HIRES_HEIGHT = 200;
constexpr int FLI_WIDTH = 296;  // FLI bug removes 24 pixels (3 chars) from left

constexpr std::size_t BITMAP_SIZE = 8000;
constexpr std::size_t SCREEN_RAM_SIZE = 1000;
constexpr std::size_t COLOR_RAM_SIZE = 1000;

constexpr int RGB_BYTES = 3;

// Blend mask for averaging two RGB values byte-by-byte
// Formula: (rgb1 & rgb2) + ((rgb1 ^ rgb2) >> 1 & RGB_BLEND_MASK)
constexpr std::uint32_t RGB_BLEND_MASK = 0x7f7f7f;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Write a single RGB pixel to the surface at (x, y).
 * @param surf Destination surface
 * @param x X coordinate (pixel)
 * @param y Y coordinate (pixel)
 * @param rgb RGB color value (0x00RRGGBB)
 */
inline void write_rgb_pixel(surface& surf, int x, int y, std::uint32_t rgb) {
    std::uint8_t pixel[RGB_BYTES] = {
        static_cast<std::uint8_t>((rgb >> 16) & 0xff),  // R
        static_cast<std::uint8_t>((rgb >> 8) & 0xff),   // G
        static_cast<std::uint8_t>(rgb & 0xff),          // B
    };
    // x * RGB_BYTES because write_pixels expects byte offset
    surf.write_pixels(x * RGB_BYTES, y, RGB_BYTES, pixel);
}

// ----------------------------------------------------------------------------
// color_output-aware emission
//
// C64 images use the fixed 16-colour palette, so under color_output::source we
// emit indexed8 + that palette; rgb/rgba expand through PALETTE. These helpers
// let the shared decoders honour the requested representation centrally.
// ----------------------------------------------------------------------------

// Configure the surface for the chosen output (and install the 16-colour
// palette for indexed8). Returns false on allocation failure.
inline bool setup_surface(surface& surf, int w, int h, color_output out) {
    const pixel_format fmt = out == color_output::source ? pixel_format::indexed8
                           : out == color_output::rgb    ? pixel_format::rgb888
                                                         : pixel_format::rgba8888;
    if (!surf.set_size(w, h, fmt)) return false;
    if (out == color_output::source) {
        std::uint8_t pal[16 * 3];
        for (int i = 0; i < 16; ++i) {
            pal[i * 3 + 0] = static_cast<std::uint8_t>((PALETTE[i] >> 16) & 0xff);
            pal[i * 3 + 1] = static_cast<std::uint8_t>((PALETTE[i] >> 8) & 0xff);
            pal[i * 3 + 2] = static_cast<std::uint8_t>(PALETTE[i] & 0xff);
        }
        surf.set_palette_size(16);
        surf.write_palette(0, std::span<const std::uint8_t>(pal, sizeof pal));
    }
    return true;
}

// Write a pixel given its C64 palette index (0-15), honouring the output mode.
inline void write_index_pixel(surface& surf, int x, int y, std::uint8_t index, color_output out) {
    const std::uint8_t i = index & 0x0f;
    if (out == color_output::source) {
        surf.write_pixels(x, y, 1, &i);
        return;
    }
    const std::uint32_t rgb = PALETTE[i];
    if (out == color_output::rgb) {
        write_rgb_pixel(surf, x, y, rgb);
    } else {
        std::uint8_t px[4] = {
            static_cast<std::uint8_t>((rgb >> 16) & 0xff),
            static_cast<std::uint8_t>((rgb >> 8) & 0xff),
            static_cast<std::uint8_t>(rgb & 0xff),
            0xff,
        };
        surf.write_pixels(x * 4, y, 4, px);
    }
}

// Emit a fully-rendered RGB framebuffer (e.g. from blended interlaced formats)
// honouring color_output. For source, builds a palette from the unique colours
// (<=256); if there are more, falls back to rgb888. Returns false on failure.
inline bool emit_rgb_framebuffer(surface& surf, const std::uint32_t* rgb,
                                 int w, int h, color_output out) {
    const std::size_t count = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);

    if (out == color_output::source) {
        // Build a small palette of unique colours (<=256).
        std::uint32_t colors[256];
        int n = 0;
        std::vector<std::uint8_t> indices(count);
        bool overflow = false;
        for (std::size_t p = 0; p < count; ++p) {
            const std::uint32_t c = rgb[p] & 0xffffff;
            int idx = -1;
            for (int k = 0; k < n; ++k) {
                if (colors[k] == c) { idx = k; break; }
            }
            if (idx < 0) {
                if (n >= 256) { overflow = true; break; }
                colors[n] = c;
                idx = n++;
            }
            indices[p] = static_cast<std::uint8_t>(idx);
        }
        if (!overflow) {
            if (!surf.set_size(w, h, pixel_format::indexed8)) return false;
            std::vector<std::uint8_t> pal(static_cast<std::size_t>(n) * 3);
            for (int k = 0; k < n; ++k) {
                pal[k * 3 + 0] = static_cast<std::uint8_t>((colors[k] >> 16) & 0xff);
                pal[k * 3 + 1] = static_cast<std::uint8_t>((colors[k] >> 8) & 0xff);
                pal[k * 3 + 2] = static_cast<std::uint8_t>(colors[k] & 0xff);
            }
            surf.set_palette_size(n);
            surf.write_palette(0, pal);
            for (int y = 0; y < h; ++y) {
                surf.write_pixels(0, y, w, &indices[static_cast<std::size_t>(y) * w]);
            }
            return true;
        }
        // too many colours: fall through to rgb expansion
    }

    const bool rgba = out == color_output::rgba;
    if (!surf.set_size(w, h, rgba ? pixel_format::rgba8888 : pixel_format::rgb888)) return false;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::uint32_t c = rgb[static_cast<std::size_t>(y) * w + x];
            if (rgba) {
                std::uint8_t px[4] = {
                    static_cast<std::uint8_t>((c >> 16) & 0xff),
                    static_cast<std::uint8_t>((c >> 8) & 0xff),
                    static_cast<std::uint8_t>(c & 0xff), 0xff};
                surf.write_pixels(x * 4, y, 4, px);
            } else {
                write_rgb_pixel(surf, x, y, c);
            }
        }
    }
    return true;
}

/**
 * Blend two RGB colors using byte-by-byte averaging.
 * Used for interlaced/IFLI formats.
 * @param rgb1 First RGB color
 * @param rgb2 Second RGB color
 * @return Blended RGB color
 */
constexpr std::uint32_t blend_rgb(std::uint32_t rgb1, std::uint32_t rgb2) {
    return (rgb1 & rgb2) + ((rgb1 ^ rgb2) >> 1 & RGB_BLEND_MASK);
}

/**
 * Decode C64 multicolor bitmap to surface.
 *
 * C64 multicolor mode: each 4x8 pixel cell can use 4 colors
 * - Color 0 (00): background color
 * - Color 1 (01): upper nibble of screen RAM
 * - Color 2 (10): lower nibble of screen RAM
 * - Color 3 (11): lower nibble of color RAM
 *
 * @param bitmap Pointer to 8000-byte bitmap data
 * @param screen_ram Pointer to 1000-byte screen RAM
 * @param color_ram Pointer to 1000-byte color RAM
 * @param background Background color index (0-15)
 * @param surf Destination surface (must be 320x200 RGB)
 */
inline void decode_multicolor(const std::uint8_t* bitmap,
                              const std::uint8_t* screen_ram,
                              const std::uint8_t* color_ram,
                              std::uint8_t background,
                              surface& surf,
                              color_output out = color_output::rgb) {
    for (int y = 0; y < MULTICOLOR_HEIGHT; ++y) {
        for (int x = 0; x < MULTICOLOR_WIDTH; ++x) {
            // Calculate character cell position
            const std::size_t char_col = static_cast<std::size_t>(x / 8);
            const std::size_t char_row = static_cast<std::size_t>(y / 8);
            const std::size_t char_offset = char_row * 40 + char_col;

            // Get the bitmap byte for this row within the character
            const std::size_t row_in_char = static_cast<std::size_t>(y % 8);
            const std::size_t bitmap_offset = char_offset * 8 + row_in_char;
            const std::uint8_t bitmap_byte = bitmap[bitmap_offset];

            // Get the 2-bit color selector for this pixel pair
            // Pixels are in pairs: bits 7-6, 5-4, 3-2, 1-0
            const int pixel_pair = (x % 8) / 2;
            const int shift = 6 - (pixel_pair * 2);
            const int color_selector = (bitmap_byte >> shift) & 0x03;

            // Determine the color index
            std::uint8_t color_index;
            switch (color_selector) {
                case 0:
                    color_index = background & 0x0f;
                    break;
                case 1:
                    color_index = (screen_ram[char_offset] >> 4) & 0x0f;
                    break;
                case 2:
                    color_index = screen_ram[char_offset] & 0x0f;
                    break;
                case 3:
                    color_index = color_ram[char_offset] & 0x0f;
                    break;
                default:
                    color_index = 0;
                    break;
            }

            write_index_pixel(surf, x, y, color_index, out);
        }
    }
}

/**
 * Decode C64 hires bitmap to surface.
 *
 * @param bitmap Pointer to 8000-byte bitmap data
 * @param video_matrix Pointer to 1000-byte video matrix (nullptr for fixed colors)
 * @param fixed_colors If video_matrix is nullptr, use this byte for colors
 *                     (upper nibble = foreground, lower nibble = background)
 * @param surf Destination surface (must be 320x200 RGB)
 */
inline void decode_hires(const std::uint8_t* bitmap,
                         const std::uint8_t* video_matrix,
                         std::uint8_t fixed_colors,
                         surface& surf,
                         color_output out = color_output::rgb) {
    constexpr int stride = 40;  // Characters per row

    for (int y = 0; y < HIRES_HEIGHT; ++y) {
        for (int x = 0; x < HIRES_WIDTH; ++x) {
            // Calculate bitmap byte offset
            // Bitmap is organized as 8 consecutive bytes per character cell
            const std::size_t char_row = static_cast<std::size_t>(y / 8);
            const std::size_t char_col = static_cast<std::size_t>(x / 8);
            const std::size_t row_in_char = static_cast<std::size_t>(y % 8);
            const std::size_t offset = char_row * stride * 8 + char_col * 8 + row_in_char;

            // Get the bit for this pixel (bit 7 is leftmost)
            const int bit_pos = 7 - (x & 7);
            const int bit = (bitmap[offset] >> bit_pos) & 1;

            // Get color value
            std::uint8_t color_byte;
            if (video_matrix != nullptr) {
                const std::size_t char_offset = char_row * stride + char_col;
                color_byte = video_matrix[char_offset];
            } else {
                color_byte = fixed_colors;
            }

            // Select color based on bit
            // bit 0 (background): lower nibble
            // bit 1 (foreground): upper nibble
            const std::uint8_t color_index = (bit == 0)
                ? (color_byte & 0x0f)
                : ((color_byte >> 4) & 0x0f);

            write_index_pixel(surf, x, y, color_index, out);
        }
    }
}

} // namespace onyx_image::c64

#endif // ONYX_IMAGE_CODECS_C64_COMMON_HPP_
