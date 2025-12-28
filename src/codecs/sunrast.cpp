#include <onyx_image/codecs/sunrast.hpp>
#include "byte_io.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace onyx_image {

namespace {

// Sun Raster constants
constexpr std::uint32_t RAS_MAGIC = 0x59a66a95;
constexpr std::uint32_t RT_OLD = 0;
constexpr std::uint32_t RT_STANDARD = 1;
constexpr std::uint32_t RT_BYTE_ENCODED = 2;
constexpr std::uint32_t RT_RGB = 3;
constexpr std::uint32_t RMT_NONE = 0;
constexpr std::uint32_t RMT_EQUAL_RGB = 1;
constexpr std::uint8_t RLE_FLAG = 0x80;

struct ras_info {
    int width = 0;
    int height = 0;
    int depth = 0;
    std::uint32_t length = 0;
    std::uint32_t type = 0;
    std::uint32_t colormap_type = 0;
    std::uint32_t colormap_length = 0;
    bool is_rgb = false;  // true if RGB order, false if BGR
};

// Calculate row size with padding (rows are padded to 16-bit boundary)
std::size_t row_stride(int width, int depth) {
    std::size_t bits = static_cast<std::size_t>(width) * static_cast<std::size_t>(depth);
    return ((bits + 15) / 16) * 2;
}

bool parse_header(std::span<const std::uint8_t> data, ras_info& info) {
    if (data.size() < 32) {
        return false;
    }

    const std::uint8_t* ptr = data.data();

    // Parse big-endian header manually
    std::uint32_t magic = read_be32(ptr);
    if (magic != RAS_MAGIC) {
        return false;
    }

    info.width = static_cast<int>(read_be32(ptr + 4));
    info.height = static_cast<int>(read_be32(ptr + 8));
    info.depth = static_cast<int>(read_be32(ptr + 12));
    info.length = read_be32(ptr + 16);
    info.type = read_be32(ptr + 20);
    info.colormap_type = read_be32(ptr + 24);
    info.colormap_length = read_be32(ptr + 28);
    info.is_rgb = (info.type == RT_RGB);

    return true;
}

// Returns true if decompression succeeded fully, false if truncated
bool decode_rle(const std::uint8_t* src, std::size_t src_size,
                std::vector<std::uint8_t>& dest, std::size_t dest_size) {
    dest.clear();
    dest.reserve(dest_size);

    const std::uint8_t* end = src + src_size;

    while (src < end && dest.size() < dest_size) {
        std::uint8_t byte = *src++;

        if (byte == RLE_FLAG) {
            if (src >= end) {
                return false;  // Truncated
            }
            std::uint8_t count = *src++;

            if (count == 0) {
                // Literal 0x80 byte
                dest.push_back(RLE_FLAG);
            } else {
                // Run of (count + 1) bytes
                if (src >= end) {
                    return false;  // Truncated
                }
                std::uint8_t value = *src++;
                std::size_t run_length = static_cast<std::size_t>(count) + 1;
                std::size_t remaining = dest_size - dest.size();
                std::size_t to_write = std::min(run_length, remaining);
                for (std::size_t i = 0; i < to_write; i++) {
                    dest.push_back(value);
                }
            }
        } else {
            // Literal byte
            dest.push_back(byte);
        }
    }

    return dest.size() == dest_size;
}

} // namespace

bool sunrast_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    if (data.size() < 4) {
        return false;
    }
    // Check big-endian magic number: 0x59a66a95
    return data[0] == 0x59 && data[1] == 0xa6 && data[2] == 0x6a && data[3] == 0x95;
}

decode_result sunrast_decoder::decode(std::span<const std::uint8_t> data,
                                       surface& surf,
                                       const decode_options& options) {
    if (!sniff(data)) {
        return decode_result::failure(decode_error::invalid_format, "Not a valid Sun Raster file");
    }

    ras_info info;
    try {
        if (!parse_header(data, info)) {
            return decode_result::failure(decode_error::invalid_format, "Failed to parse Sun Raster header");
        }
    } catch (const std::exception& e) {
        return decode_result::failure(decode_error::invalid_format, e.what());
    }

    if (info.width <= 0 || info.height <= 0) {
        return decode_result::failure(decode_error::invalid_format, "Invalid image dimensions");
    }

    // Check dimension limits
    const int max_w = options.max_width > 0 ? options.max_width : 16384;
    const int max_h = options.max_height > 0 ? options.max_height : 16384;
    if (info.width > max_w || info.height > max_h) {
        return decode_result::failure(decode_error::dimensions_exceeded, "Image dimensions exceed limits");
    }

    // Validate depth
    if (info.depth != 1 && info.depth != 4 && info.depth != 8 && info.depth != 24 && info.depth != 32) {
        return decode_result::failure(decode_error::invalid_format,
            "Unsupported bit depth: " + std::to_string(info.depth));
    }

    // Validate type
    if (info.type != RT_OLD && info.type != RT_STANDARD &&
        info.type != RT_BYTE_ENCODED && info.type != RT_RGB) {
        return decode_result::failure(decode_error::invalid_format,
            "Unsupported raster type: " + std::to_string(info.type));
    }

    // Calculate data positions
    const std::size_t header_size = 32;
    const std::size_t colormap_offset = header_size;
    const std::size_t pixel_offset = header_size + info.colormap_length;

    if (pixel_offset > data.size()) {
        return decode_result::failure(decode_error::truncated_data,
            "Sun Raster data truncated: incomplete file header");
    }

    // Read colormap if present
    std::vector<std::uint8_t> palette;
    if (info.colormap_type == RMT_EQUAL_RGB && info.colormap_length > 0) {
        const std::size_t num_colors = info.colormap_length / 3;
        palette.resize(num_colors * 3);

        // Sun Raster stores colormap as separate R, G, B planes
        const std::uint8_t* cmap = data.data() + colormap_offset;
        for (std::size_t i = 0; i < num_colors; i++) {
            palette[i * 3 + 0] = cmap[i];                    // R
            palette[i * 3 + 1] = cmap[num_colors + i];       // G
            palette[i * 3 + 2] = cmap[num_colors * 2 + i];   // B
        }
    }

    // Get pixel data
    const std::uint8_t* pixel_data = data.data() + pixel_offset;
    std::size_t pixel_data_size = data.size() - pixel_offset;

    // Calculate expected uncompressed size
    const std::size_t stride = row_stride(info.width, info.depth);
    const std::size_t expected_size = stride * static_cast<std::size_t>(info.height);

    // Decompress if RLE
    std::vector<std::uint8_t> decompressed;
    if (info.type == RT_BYTE_ENCODED) {
        if (!decode_rle(pixel_data, pixel_data_size, decompressed, expected_size)) {
            return decode_result::failure(decode_error::truncated_data,
                "RLE decompression failed - truncated data");
        }
        pixel_data = decompressed.data();
        pixel_data_size = decompressed.size();
    }

    // Determine output format
    pixel_format out_format;
    if ((info.depth == 8 || info.depth == 4 || info.depth == 1) && !palette.empty()) {
        out_format = pixel_format::indexed8;
    } else if (info.depth == 1) {
        out_format = pixel_format::indexed8;
        // Create default black/white palette
        palette = {0, 0, 0, 255, 255, 255};
    } else {
        out_format = pixel_format::rgba8888;
    }

    if (!surf.set_size(info.width, info.height, out_format)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    // Set palette if indexed
    if (out_format == pixel_format::indexed8 && !palette.empty()) {
        surf.set_palette_size(static_cast<int>(palette.size() / 3));
        surf.write_palette(0, palette);
    }

    // Decode pixel data
    std::vector<std::uint8_t> row_buffer(static_cast<std::size_t>(info.width) * 4);

    for (int y = 0; y < info.height; y++) {
        const std::uint8_t* src_row = pixel_data + static_cast<std::size_t>(y) * stride;

        if (src_row + stride > pixel_data + pixel_data_size) {
            return decode_result::failure(decode_error::truncated_data, "Unexpected end of data");
        }

        if (info.depth == 1) {
            // 1-bit: MSB first
            for (std::size_t x = 0; x < static_cast<std::size_t>(info.width); x++) {
                std::size_t byte_idx = x / 8;
                int bit_idx = 7 - static_cast<int>(x % 8);
                row_buffer[x] = (src_row[byte_idx] >> bit_idx) & 0x01;
            }
            surf.write_pixels(0, y, info.width, row_buffer.data());
        } else if (info.depth == 4) {
            // 4-bit: high nibble first
            for (std::size_t x = 0; x < static_cast<std::size_t>(info.width); x++) {
                std::size_t byte_idx = x / 2;
                if (x % 2 == 0) {
                    row_buffer[x] = (src_row[byte_idx] >> 4) & 0x0F;
                } else {
                    row_buffer[x] = src_row[byte_idx] & 0x0F;
                }
            }
            surf.write_pixels(0, y, info.width, row_buffer.data());
        } else if (info.depth == 8) {
            // 8-bit indexed
            surf.write_pixels(0, y, info.width, src_row);
        } else if (info.depth == 24) {
            // 24-bit: BGR or RGB
            for (std::size_t x = 0; x < static_cast<std::size_t>(info.width); x++) {
                if (info.is_rgb) {
                    row_buffer[x * 4 + 0] = src_row[x * 3 + 0];  // R
                    row_buffer[x * 4 + 1] = src_row[x * 3 + 1];  // G
                    row_buffer[x * 4 + 2] = src_row[x * 3 + 2];  // B
                } else {
                    row_buffer[x * 4 + 0] = src_row[x * 3 + 2];  // R (from B position)
                    row_buffer[x * 4 + 1] = src_row[x * 3 + 1];  // G
                    row_buffer[x * 4 + 2] = src_row[x * 3 + 0];  // B (from R position)
                }
                row_buffer[x * 4 + 3] = 0xFF;
            }
            surf.write_pixels(0, y, info.width * 4, row_buffer.data());
        } else if (info.depth == 32) {
            // 32-bit: XBGR or XRGB (first byte is padding)
            for (std::size_t x = 0; x < static_cast<std::size_t>(info.width); x++) {
                if (info.is_rgb) {
                    row_buffer[x * 4 + 0] = src_row[x * 4 + 1];  // R
                    row_buffer[x * 4 + 1] = src_row[x * 4 + 2];  // G
                    row_buffer[x * 4 + 2] = src_row[x * 4 + 3];  // B
                } else {
                    row_buffer[x * 4 + 0] = src_row[x * 4 + 3];  // R (from B position)
                    row_buffer[x * 4 + 1] = src_row[x * 4 + 2];  // G
                    row_buffer[x * 4 + 2] = src_row[x * 4 + 1];  // B (from R position)
                }
                row_buffer[x * 4 + 3] = 0xFF;
            }
            surf.write_pixels(0, y, info.width * 4, row_buffer.data());
        }
    }

    return decode_result::success();
}

} // namespace onyx_image
