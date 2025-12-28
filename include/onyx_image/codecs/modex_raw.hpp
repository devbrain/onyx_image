#ifndef ONYX_IMAGE_CODECS_MODEX_RAW_HPP_
#define ONYX_IMAGE_CODECS_MODEX_RAW_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <span>

namespace onyx_image {

// ============================================================================
// Raw Mode X Data Decoder
// ============================================================================
//
// Decodes raw VGA Mode X (unchained 256-color) graphics data.
// Mode X uses 4 planes where each plane contains every 4th pixel:
//   Plane 0: pixels 0, 4, 8, 12, ...
//   Plane 1: pixels 1, 5, 9, 13, ...
//   Plane 2: pixels 2, 6, 10, 14, ...
//   Plane 3: pixels 3, 7, 11, 15, ...
//
// Common resolutions: 320x200, 320x240, 320x400, 360x480
//
// Reference: PC Game Programmer's Encyclopedia - Mode X

// ----------------------------------------------------------------------------
// Mode X Format Types
// ----------------------------------------------------------------------------

enum class modex_format {
    // Full-planar: All pixels of plane 0, then plane 1, etc.
    // Layout: [plane0: W/4*H bytes][plane1: W/4*H bytes]...
    graphic_planar,

    // Row-planar: For each row, all four planes for that row.
    // Layout: [row0: p0,p1,p2,p3][row1: p0,p1,p2,p3]...
    row_planar,

    // Byte-planar (interleaved): For each 4 horizontal pixels, one byte per plane.
    // Layout: [4px: p0,p1,p2,p3][next 4px: p0,p1,p2,p3]...
    byte_planar,

    // Linear: Standard Mode 13h style (not actually Mode X, but useful for comparison)
    // Each byte is one pixel, sequential left-to-right, top-to-bottom.
    linear
};

// ----------------------------------------------------------------------------
// Decode Options
// ----------------------------------------------------------------------------

struct modex_raw_options {
    int width = 320;              // Image width (must be multiple of 4 for planar formats)
    int height = 240;             // Image height
    modex_format format = modex_format::graphic_planar;
};

// ----------------------------------------------------------------------------
// Decode Functions
// ----------------------------------------------------------------------------

/**
 * Decode raw Mode X data to a surface.
 * @param data Raw Mode X pixel data
 * @param surf Destination surface (will be set to indexed8 format)
 * @param opts Format options specifying dimensions and layout
 * @return Decode result
 */
[[nodiscard]] ONYX_IMAGE_EXPORT
decode_result decode_modex_raw(std::span<const std::uint8_t> data,
                                surface& surf,
                                const modex_raw_options& opts);

/**
 * Decode graphic-planar Mode X data (full planes stored sequentially).
 * @param data Raw pixel data
 * @param surf Destination surface
 * @param width Image width (should be multiple of 4)
 * @param height Image height
 * @return Decode result
 */
[[nodiscard]] ONYX_IMAGE_EXPORT
decode_result decode_modex_graphic_planar(std::span<const std::uint8_t> data,
                                           surface& surf,
                                           int width, int height);

/**
 * Decode row-planar Mode X data (planes interleaved per row).
 * @param data Raw pixel data
 * @param surf Destination surface
 * @param width Image width (should be multiple of 4)
 * @param height Image height
 * @return Decode result
 */
[[nodiscard]] ONYX_IMAGE_EXPORT
decode_result decode_modex_row_planar(std::span<const std::uint8_t> data,
                                       surface& surf,
                                       int width, int height);

/**
 * Decode byte-planar Mode X data (planes interleaved per 4 pixels).
 * @param data Raw pixel data
 * @param surf Destination surface
 * @param width Image width (should be multiple of 4)
 * @param height Image height
 * @return Decode result
 */
[[nodiscard]] ONYX_IMAGE_EXPORT
decode_result decode_modex_byte_planar(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        int width, int height);

/**
 * Decode linear 256-color data (Mode 13h style).
 * @param data Raw pixel data
 * @param surf Destination surface
 * @param width Image width
 * @param height Image height
 * @return Decode result
 */
[[nodiscard]] ONYX_IMAGE_EXPORT
decode_result decode_modex_linear(std::span<const std::uint8_t> data,
                                   surface& surf,
                                   int width, int height);

// ----------------------------------------------------------------------------
// Utility Functions
// ----------------------------------------------------------------------------

/**
 * Calculate required data size for given dimensions and format.
 * @param width Image width
 * @param height Image height
 * @param format Mode X format
 * @return Required size in bytes
 */
[[nodiscard]] ONYX_IMAGE_EXPORT
constexpr std::size_t modex_raw_data_size(int width, int height,
                                           modex_format format) noexcept {
    if (width <= 0 || height <= 0) {
        return 0;
    }

    std::size_t w = static_cast<std::size_t>(width);
    std::size_t h = static_cast<std::size_t>(height);

    switch (format) {
        case modex_format::graphic_planar:
        case modex_format::row_planar:
        case modex_format::byte_planar:
            // Planar formats: 4 planes, each plane has width/4 bytes per row
            // For non-multiple-of-4 widths, round up
            return ((w + 3) / 4) * h * 4;

        case modex_format::linear:
            // Linear: 1 byte per pixel
            return w * h;
    }

    return 0;
}

/**
 * Get which plane a given X coordinate belongs to.
 * @param x X coordinate
 * @return Plane number (0-3)
 */
[[nodiscard]] constexpr int modex_plane_for_x(int x) noexcept {
    return x & 3;
}

/**
 * Get byte offset within a plane for a given X coordinate.
 * @param x X coordinate
 * @return Byte offset within plane
 */
[[nodiscard]] constexpr int modex_offset_for_x(int x) noexcept {
    return x >> 2;  // x / 4
}

} // namespace onyx_image

#endif // ONYX_IMAGE_CODECS_MODEX_RAW_HPP_
