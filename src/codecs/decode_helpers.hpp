#pragma once

#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <cstddef>

namespace onyx_image {

// Default dimension limits
constexpr int DEFAULT_MAX_DIMENSION = 16384;
constexpr int DEFAULT_ICON_MAX_DIMENSION = 256;

// Get effective dimension limits from options
inline std::pair<int, int> get_dimension_limits(const decode_options& options,
                                                 int default_limit = DEFAULT_MAX_DIMENSION) {
    int max_w = options.max_width > 0 ? options.max_width : default_limit;
    int max_h = options.max_height > 0 ? options.max_height : default_limit;
    return {max_w, max_h};
}

// Validate dimensions against limits, returning failure result if exceeded
// Returns success() if dimensions are within limits
inline decode_result validate_dimensions(int width, int height,
                                          const decode_options& options,
                                          int default_limit = DEFAULT_MAX_DIMENSION) {
    auto [max_w, max_h] = get_dimension_limits(options, default_limit);
    if (width > max_w || height > max_h) {
        return decode_result::failure(decode_error::dimensions_exceeded,
            "Image dimensions exceed limits");
    }
    return decode_result::success();
}

// Copy pixel data row-by-row to a surface
// data: pointer to pixel data (row-major, contiguous)
// row_bytes: bytes per row in the source data
// height: number of rows to copy
inline void write_rows(surface& surf, const std::uint8_t* data,
                       std::size_t row_bytes, int height) {
    for (int y = 0; y < height; ++y) {
        surf.write_pixels(0, y, static_cast<int>(row_bytes),
                          data + static_cast<std::size_t>(y) * row_bytes);
    }
}

// Row stride calculation (4-byte aligned, for BMP/ICO/DIB formats)
inline std::size_t row_stride_4byte(int width, int bits_per_pixel) {
    return ((static_cast<std::size_t>(width) * static_cast<std::size_t>(bits_per_pixel) + 31) / 32) * 4;
}

// Row stride calculation (2-byte aligned, for Sun Raster format)
inline std::size_t row_stride_2byte(int width, int bits_per_pixel) {
    std::size_t bits = static_cast<std::size_t>(width) * static_cast<std::size_t>(bits_per_pixel);
    return ((bits + 15) / 16) * 2;
}

// Extract pixel from packed data (1, 2, 4, or 8 bits per pixel)
inline std::uint8_t extract_pixel(const std::uint8_t* row, int x, int bits_per_pixel) {
    switch (bits_per_pixel) {
        case 1: {
            int byte_index = x / 8;
            int bit_index = 7 - (x % 8);
            return (row[byte_index] >> bit_index) & 0x01;
        }
        case 2: {
            int byte_index = x / 4;
            int bit_index = 6 - (x % 4) * 2;
            return (row[byte_index] >> bit_index) & 0x03;
        }
        case 4: {
            int byte_index = x / 2;
            int bit_index = (x % 2) ? 0 : 4;
            return (row[byte_index] >> bit_index) & 0x0F;
        }
        case 8:
            return row[x];
        default:
            return 0;
    }
}

} // namespace onyx_image
