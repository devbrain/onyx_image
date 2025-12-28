#ifndef ONYX_IMAGE_CODECS_EGA_RAW_HPP_
#define ONYX_IMAGE_CODECS_EGA_RAW_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <span>

namespace onyx_image {

// ============================================================================
// Raw EGA Data Decoder
// ============================================================================
//
// Decodes raw EGA graphics data in various planar and linear formats.
// EGA uses 4 color planes (Blue, Green, Red, Intensity) for 16 colors.
//
// Reference: https://moddingwiki.shikadi.net/wiki/Raw_EGA_data

// ----------------------------------------------------------------------------
// EGA Plane Order
// ----------------------------------------------------------------------------
// Standard order is B-G-R-I, but some formats use different arrangements.

enum class ega_plane_order {
    bgri,  // Blue, Green, Red, Intensity (standard EGA)
    rgbi,  // Red, Green, Blue, Intensity
    irgb,  // Intensity, Red, Green, Blue
    bgr,   // Blue, Green, Red (3-plane, 8 colors)
    rgb    // Red, Green, Blue (3-plane, 8 colors)
};

// ----------------------------------------------------------------------------
// EGA Raw Format Types
// ----------------------------------------------------------------------------

enum class ega_format {
    // Full-planar: All pixels of plane 0, then all of plane 1, etc.
    // Layout: [plane0: W*H/8 bytes][plane1: W*H/8 bytes]...
    graphic_planar,

    // Row-planar: For each row, all planes for that row sequentially.
    // Layout: [row0: plane0,plane1,plane2,plane3][row1: ...]...
    row_planar,

    // Byte-planar (interleaved): For each 8 pixels, one byte per plane.
    // Layout: [8px: p0,p1,p2,p3][next 8px: p0,p1,p2,p3]...
    byte_planar,

    // Linear: Each nibble is a complete 4-bit palette index.
    // High nibble = first pixel, low nibble = second pixel.
    linear
};

// ----------------------------------------------------------------------------
// Decode Options
// ----------------------------------------------------------------------------

struct ega_raw_options {
    int width = 320;              // Image width in pixels
    int height = 200;             // Image height in pixels
    ega_format format = ega_format::row_planar;
    ega_plane_order plane_order = ega_plane_order::bgri;
    int num_planes = 4;           // 4 for 16 colors, 3 for 8 colors, 2 for 4 colors, 1 for 2 colors
    bool high_nibble_first = true; // For linear format: high nibble is first pixel
};

// ----------------------------------------------------------------------------
// Decode Functions
// ----------------------------------------------------------------------------

/**
 * Decode raw EGA data to a surface.
 * @param data Raw EGA pixel data
 * @param surf Destination surface (will be set to indexed8 format)
 * @param opts Format options specifying dimensions and layout
 * @return Decode result
 */
[[nodiscard]] ONYX_IMAGE_EXPORT
decode_result decode_ega_raw(std::span<const std::uint8_t> data,
                              surface& surf,
                              const ega_raw_options& opts);

/**
 * Decode graphic-planar EGA data (full planes stored sequentially).
 * @param data Raw pixel data
 * @param surf Destination surface
 * @param width Image width
 * @param height Image height
 * @param num_planes Number of planes (1-4)
 * @param plane_order Plane arrangement
 * @return Decode result
 */
[[nodiscard]] ONYX_IMAGE_EXPORT
decode_result decode_ega_graphic_planar(std::span<const std::uint8_t> data,
                                         surface& surf,
                                         int width, int height,
                                         int num_planes = 4,
                                         ega_plane_order plane_order = ega_plane_order::bgri);

/**
 * Decode row-planar EGA data (planes interleaved per row).
 * @param data Raw pixel data
 * @param surf Destination surface
 * @param width Image width
 * @param height Image height
 * @param num_planes Number of planes (1-4)
 * @param plane_order Plane arrangement
 * @return Decode result
 */
[[nodiscard]] ONYX_IMAGE_EXPORT
decode_result decode_ega_row_planar(std::span<const std::uint8_t> data,
                                     surface& surf,
                                     int width, int height,
                                     int num_planes = 4,
                                     ega_plane_order plane_order = ega_plane_order::bgri);

/**
 * Decode byte-planar EGA data (planes interleaved per byte).
 * @param data Raw pixel data
 * @param surf Destination surface
 * @param width Image width
 * @param height Image height
 * @param num_planes Number of planes (1-4)
 * @param plane_order Plane arrangement
 * @return Decode result
 */
[[nodiscard]] ONYX_IMAGE_EXPORT
decode_result decode_ega_byte_planar(std::span<const std::uint8_t> data,
                                      surface& surf,
                                      int width, int height,
                                      int num_planes = 4,
                                      ega_plane_order plane_order = ega_plane_order::bgri);

/**
 * Decode linear EGA data (packed nibbles, 2 pixels per byte).
 * @param data Raw pixel data
 * @param surf Destination surface
 * @param width Image width
 * @param height Image height
 * @param high_nibble_first If true, high nibble is first pixel (default)
 * @return Decode result
 */
[[nodiscard]] ONYX_IMAGE_EXPORT
decode_result decode_ega_linear(std::span<const std::uint8_t> data,
                                 surface& surf,
                                 int width, int height,
                                 bool high_nibble_first = true);

// ----------------------------------------------------------------------------
// Utility Functions
// ----------------------------------------------------------------------------

/**
 * Calculate required data size for given dimensions and format.
 * @param width Image width
 * @param height Image height
 * @param format EGA format
 * @param num_planes Number of planes (for planar formats)
 * @return Required size in bytes
 */
[[nodiscard]] ONYX_IMAGE_EXPORT
std::size_t ega_raw_data_size(int width, int height,
                               ega_format format,
                               int num_planes = 4) noexcept;

/**
 * Get plane bit position for a given plane order.
 * Returns which bit position (0-3) each logical plane maps to.
 * @param order Plane order
 * @param plane Logical plane index (0=blue, 1=green, 2=red, 3=intensity)
 * @return Bit position in final pixel value
 */
[[nodiscard]] ONYX_IMAGE_EXPORT
constexpr int ega_plane_bit(ega_plane_order order, int plane) noexcept {
    // Returns which bit position this plane contributes to
    switch (order) {
        case ega_plane_order::bgri:
            // Plane 0=Blue->bit0, 1=Green->bit1, 2=Red->bit2, 3=Intensity->bit3
            return plane;
        case ega_plane_order::rgbi:
            // Plane 0=Red->bit2, 1=Green->bit1, 2=Blue->bit0, 3=Intensity->bit3
            switch (plane) {
                case 0: return 2;  // Red
                case 1: return 1;  // Green
                case 2: return 0;  // Blue
                default: return 3; // Intensity
            }
        case ega_plane_order::irgb:
            // Plane 0=Intensity->bit3, 1=Red->bit2, 2=Green->bit1, 3=Blue->bit0
            switch (plane) {
                case 0: return 3;  // Intensity
                case 1: return 2;  // Red
                case 2: return 1;  // Green
                default: return 0; // Blue
            }
        case ega_plane_order::bgr:
        case ega_plane_order::rgb:
            // 3-plane formats
            if (order == ega_plane_order::bgr) {
                return plane;  // B=0, G=1, R=2
            } else {
                switch (plane) {
                    case 0: return 2;  // Red
                    case 1: return 1;  // Green
                    default: return 0; // Blue
                }
            }
    }
    return plane;
}

} // namespace onyx_image

#endif // ONYX_IMAGE_CODECS_EGA_RAW_HPP_
