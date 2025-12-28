#include <onyx_image/codecs/bmp.hpp>
#include <formats/bmp/bmp.hh>
#include "decode_helpers.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace onyx_image {

namespace {

// Compression methods
constexpr std::uint32_t BI_RGB = 0;
constexpr std::uint32_t BI_RLE8 = 1;
constexpr std::uint32_t BI_RLE4 = 2;
constexpr std::uint32_t BI_BITFIELDS = 3;

// BMP signature: "BM"
constexpr std::uint8_t BMP_SIGNATURE[] = {'B', 'M'};

struct bmp_info {
    int width = 0;
    int height = 0;
    int bits_per_pixel = 0;
    std::uint32_t compression = 0;
    std::uint32_t colors_used = 0;
    std::uint32_t data_offset = 0;
    std::uint32_t header_size = 0;  // Info header size (not including file header)
    int palette_entry_size = 4;  // 3 for OS/2 1.x, 4 otherwise
    bool top_down = false;

    // Bitfield masks (for BI_BITFIELDS)
    std::uint32_t red_mask = 0;
    std::uint32_t green_mask = 0;
    std::uint32_t blue_mask = 0;
    std::uint32_t alpha_mask = 0;

    // Computed shifts and scales for bitfields
    int red_shift = 0;
    int green_shift = 0;
    int blue_shift = 0;
    int alpha_shift = 0;
    int red_scale = 0;
    int green_scale = 0;
    int blue_scale = 0;
};

// Count trailing zero bits
int count_zero_bits(std::uint32_t v) {
    if (v == 0) return 32;
    int count = 0;
    while ((v & 1) == 0) {
        count++;
        v >>= 1;
    }
    return count;
}

// Count bits set in mask
int count_mask_bits(std::uint32_t v) {
    int count = 0;
    while (v) {
        if (v & 1) count++;
        v >>= 1;
    }
    return count;
}

bool parse_header(std::span<const std::uint8_t> data, bmp_info& info) {
    if (data.size() < 14 + 12) {
        return false;
    }

    const std::uint8_t* ptr = data.data();
    const std::uint8_t* end = data.data() + data.size();

    // Parse file header
    auto file_header = formats::bmp::bmp_file_header::read(ptr, end);
    info.data_offset = file_header.data_offset;

    // Parse info header - first read size to determine type
    const std::uint8_t* info_start = ptr;
    std::uint32_t header_size = formats::bmp::read_uint32(ptr, end);
    ptr = info_start;  // Reset to read full header

    info.header_size = header_size;

    if (header_size == 12) {
        // OS/2 1.x BITMAPCOREHEADER
        auto header = formats::bmp::bmp_core_header::read(ptr, end);
        info.width = header.width;
        info.height = header.height < 0 ? -header.height : header.height;
        info.top_down = header.height < 0;
        info.bits_per_pixel = header.bits_per_pixel;
        info.compression = BI_RGB;
        info.palette_entry_size = 3;

        // OS/2 1.x has no colors_used field - calculate from available space
        // Palette starts at 14 (file header) + 12 (core header) = 26
        if (info.bits_per_pixel <= 8) {
            const std::size_t palette_start = 14 + 12;
            const std::size_t palette_bytes = info.data_offset > palette_start ?
                                               info.data_offset - palette_start : 0;
            info.colors_used = static_cast<std::uint32_t>(palette_bytes / 3);
            // Cap at max colors for bit depth
            const std::uint32_t max_colors = 1u << info.bits_per_pixel;
            if (info.colors_used > max_colors) {
                info.colors_used = max_colors;
            }
        }
    } else if (header_size == 64) {
        // OS/2 2.x header
        auto header = formats::bmp::bmp_os2_v2_header::read(ptr, end);
        info.width = static_cast<int>(header.width);
        info.height = static_cast<int>(header.height);
        info.top_down = false;  // OS/2 2.x is always bottom-up
        info.bits_per_pixel = header.bits_per_pixel;
        info.compression = header.compression;
        info.colors_used = header.colors_used;

        if (info.colors_used == 0 && info.bits_per_pixel <= 8) {
            info.colors_used = 1u << info.bits_per_pixel;
        }

        // OS/2 2.x can use either 3-byte or 4-byte palette entries
        // Detect based on available space
        if (info.bits_per_pixel <= 8 && info.colors_used > 0) {
            const std::size_t palette_start = 14 + 64;
            const std::size_t palette_bytes = info.data_offset > palette_start ?
                                               info.data_offset - palette_start : 0;
            const std::size_t bytes_per_color = palette_bytes / info.colors_used;
            info.palette_entry_size = (bytes_per_color >= 4) ? 4 : 3;
        } else {
            info.palette_entry_size = 4;
        }
    } else if (header_size >= 40) {
        // Windows BITMAPINFOHEADER or later
        if (header_size >= 108) {
            auto header = formats::bmp::bmp_v4_header::read(ptr, end);
            info.width = header.width;
            info.height = header.height < 0 ? -header.height : header.height;
            info.top_down = header.height < 0;
            info.bits_per_pixel = header.bits_per_pixel;
            info.compression = header.compression;
            info.colors_used = header.colors_used;
            info.red_mask = header.red_mask;
            info.green_mask = header.green_mask;
            info.blue_mask = header.blue_mask;
            info.alpha_mask = header.alpha_mask;
        } else if (header_size >= 56) {
            auto header = formats::bmp::bmp_v3_header::read(ptr, end);
            info.width = header.width;
            info.height = header.height < 0 ? -header.height : header.height;
            info.top_down = header.height < 0;
            info.bits_per_pixel = header.bits_per_pixel;
            info.compression = header.compression;
            info.colors_used = header.colors_used;
            info.red_mask = header.red_mask;
            info.green_mask = header.green_mask;
            info.blue_mask = header.blue_mask;
            info.alpha_mask = header.alpha_mask;
        } else if (header_size >= 52) {
            auto header = formats::bmp::bmp_v2_header::read(ptr, end);
            info.width = header.width;
            info.height = header.height < 0 ? -header.height : header.height;
            info.top_down = header.height < 0;
            info.bits_per_pixel = header.bits_per_pixel;
            info.compression = header.compression;
            info.colors_used = header.colors_used;
            info.red_mask = header.red_mask;
            info.green_mask = header.green_mask;
            info.blue_mask = header.blue_mask;
        } else {
            auto header = formats::bmp::bmp_info_header::read(ptr, end);
            info.width = header.width;
            info.height = header.height < 0 ? -header.height : header.height;
            info.top_down = header.height < 0;
            info.bits_per_pixel = header.bits_per_pixel;
            info.compression = header.compression;
            info.colors_used = header.colors_used;
        }
        info.palette_entry_size = 4;

        if (info.colors_used == 0 && info.bits_per_pixel <= 8) {
            info.colors_used = 1u << info.bits_per_pixel;
        }
    } else {
        return false;
    }

    // Handle bitfields
    if (info.compression == BI_BITFIELDS) {
        // Masks might be in header or after it
        if (info.red_mask == 0 && info.green_mask == 0 && info.blue_mask == 0) {
            // Read masks from after header
            if (ptr + 12 <= end) {
                info.red_mask = formats::bmp::read_uint32(ptr, end);
                info.green_mask = formats::bmp::read_uint32(ptr, end);
                info.blue_mask = formats::bmp::read_uint32(ptr, end);
            }
        }

        info.red_shift = count_zero_bits(info.red_mask);
        info.green_shift = count_zero_bits(info.green_mask);
        info.blue_shift = count_zero_bits(info.blue_mask);

        info.red_scale = 8 - count_mask_bits(info.red_mask);
        info.green_scale = 8 - count_mask_bits(info.green_mask);
        info.blue_scale = 8 - count_mask_bits(info.blue_mask);
    } else if (info.bits_per_pixel == 16) {
        // Default 16-bit format: 5-5-5
        info.red_mask = 0x7C00;
        info.green_mask = 0x03E0;
        info.blue_mask = 0x001F;
        info.red_shift = 10;
        info.green_shift = 5;
        info.blue_shift = 0;
        info.red_scale = 3;
        info.green_scale = 3;
        info.blue_scale = 3;
    }

    return true;
}

void decode_rle8(const std::uint8_t* src, std::size_t src_size,
                 std::vector<std::uint8_t>& indices,
                 int width, int height) {
    indices.resize(static_cast<std::size_t>(width) * height, 0);
    const std::uint8_t* end = src + src_size;

    int x = 0;
    int y = 0;

    while (src + 1 < end && y < height) {
        std::uint8_t count = *src++;
        std::uint8_t value = *src++;

        if (count == 0) {
            // Escape code
            if (value == 0) {
                // End of line
                x = 0;
                y++;
            } else if (value == 1) {
                // End of bitmap
                break;
            } else if (value == 2) {
                // Delta
                if (src + 1 < end) {
                    x += *src++;
                    y += *src++;
                }
            } else {
                // Absolute mode
                for (int i = 0; i < value && src < end && y < height; i++) {
                    if (x < width) {
                        indices[static_cast<std::size_t>(y) * width + x] = *src;
                        x++;
                    }
                    src++;
                }
                // Pad to word boundary
                if (value & 1) src++;
            }
        } else {
            // Run of pixels
            for (int i = 0; i < count && y < height; i++) {
                if (x < width) {
                    indices[static_cast<std::size_t>(y) * width + x] = value;
                    x++;
                }
            }
        }
    }
}

void decode_rle4(const std::uint8_t* src, std::size_t src_size,
                 std::vector<std::uint8_t>& indices,
                 int width, int height) {
    indices.resize(static_cast<std::size_t>(width) * height, 0);
    const std::uint8_t* end = src + src_size;

    int x = 0;
    int y = 0;

    while (src + 1 < end && y < height) {
        std::uint8_t count = *src++;
        std::uint8_t value = *src++;

        if (count == 0) {
            // Escape code
            if (value == 0) {
                // End of line
                x = 0;
                y++;
            } else if (value == 1) {
                // End of bitmap
                break;
            } else if (value == 2) {
                // Delta
                if (src + 1 < end) {
                    x += *src++;
                    y += *src++;
                }
            } else {
                // Absolute mode
                int pixels_read = 0;
                for (int i = 0; i < value && y < height; i++) {
                    if (pixels_read % 2 == 0) {
                        if (src >= end) break;
                    }
                    std::uint8_t nibble;
                    if (i % 2 == 0) {
                        nibble = (*src >> 4) & 0x0F;
                    } else {
                        nibble = *src & 0x0F;
                        src++;
                    }
                    if (x < width) {
                        indices[static_cast<std::size_t>(y) * width + x] = nibble;
                        x++;
                    }
                    pixels_read++;
                }
                if (value % 2 == 1) src++;
                // Pad to word boundary
                int bytes_read = (value + 1) / 2;
                if (bytes_read & 1) src++;
            }
        } else {
            // Run of pixels (alternating nibbles)
            std::uint8_t hi = (value >> 4) & 0x0F;
            std::uint8_t lo = value & 0x0F;
            for (int i = 0; i < count && y < height; i++) {
                if (x < width) {
                    indices[static_cast<std::size_t>(y) * width + x] = (i % 2 == 0) ? hi : lo;
                    x++;
                }
            }
        }
    }
}

} // namespace

bool bmp_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    if (data.size() < 2) {
        return false;
    }
    return data[0] == BMP_SIGNATURE[0] && data[1] == BMP_SIGNATURE[1];
}

decode_result bmp_decoder::decode(std::span<const std::uint8_t> data,
                                   surface& surf,
                                   const decode_options& options) {
    if (!sniff(data)) {
        return decode_result::failure(decode_error::invalid_format, "Not a valid BMP file");
    }

    bmp_info info;
    try {
        if (!parse_header(data, info)) {
            return decode_result::failure(decode_error::invalid_format, "Failed to parse BMP header");
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

    // Validate data offset
    if (info.data_offset >= data.size()) {
        return decode_result::failure(decode_error::truncated_data, "Invalid data offset");
    }

    const std::uint8_t* pixel_data = data.data() + info.data_offset;
    const std::size_t pixel_data_size = data.size() - info.data_offset;

    // Read palette if present
    std::vector<std::uint8_t> palette;
    if (info.bits_per_pixel <= 8 && info.colors_used > 0) {
        // Palette starts after file header (14 bytes) + info header
        const std::size_t palette_offset = 14 + info.header_size;
        const std::size_t palette_size = info.colors_used * info.palette_entry_size;

        if (palette_offset + palette_size <= info.data_offset && palette_offset + palette_size <= data.size()) {
            palette.resize(info.colors_used * 3);
            const std::uint8_t* pal_ptr = data.data() + palette_offset;
            for (std::uint32_t i = 0; i < info.colors_used; i++) {
                palette[i * 3 + 2] = pal_ptr[0];  // Blue -> RGB order
                palette[i * 3 + 1] = pal_ptr[1];  // Green
                palette[i * 3 + 0] = pal_ptr[2];  // Red
                pal_ptr += info.palette_entry_size;
            }
        }
    }

    // Determine output format
    pixel_format out_format;
    if (info.bits_per_pixel <= 8 && !palette.empty()) {
        out_format = pixel_format::indexed8;
    } else {
        out_format = pixel_format::rgba8888;
    }

    if (!surf.set_size(info.width, info.height, out_format)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    // Set palette if indexed
    if (out_format == pixel_format::indexed8 && !palette.empty()) {
        surf.set_palette_size(static_cast<int>(info.colors_used));
        surf.write_palette(0, palette);
    }

    // Handle RLE compression
    if (info.compression == BI_RLE8 || info.compression == BI_RLE4) {
        std::vector<std::uint8_t> indices;
        if (info.compression == BI_RLE8) {
            decode_rle8(pixel_data, pixel_data_size, indices, info.width, info.height);
        } else {
            decode_rle4(pixel_data, pixel_data_size, indices, info.width, info.height);
        }

        // Write to surface (RLE is always bottom-up, need to flip)
        for (int y = 0; y < info.height; y++) {
            int src_y = info.height - 1 - y;
            surf.write_pixels(0, y, info.width, indices.data() + static_cast<std::size_t>(src_y) * info.width);
        }
        return decode_result::success();
    }

    // Uncompressed data
    const std::size_t src_row_size = row_stride_4byte(info.width, info.bits_per_pixel);
    std::vector<std::uint8_t> row_buffer(static_cast<std::size_t>(info.width) * 4);

    for (int y = 0; y < info.height; y++) {
        int src_y = info.top_down ? y : (info.height - 1 - y);
        const std::uint8_t* src_row = pixel_data + static_cast<std::size_t>(src_y) * src_row_size;

        if (src_row + src_row_size > data.data() + data.size()) {
            return decode_result::failure(decode_error::truncated_data, "Unexpected end of data");
        }

        if (info.bits_per_pixel <= 8) {
            // Indexed mode
            for (int x = 0; x < info.width; x++) {
                row_buffer[x] = extract_pixel(src_row, x, info.bits_per_pixel);
            }
            surf.write_pixels(0, y, info.width, row_buffer.data());
        } else if (info.bits_per_pixel == 16) {
            // 16-bit RGB
            for (int x = 0; x < info.width; x++) {
                std::uint16_t pixel = src_row[x * 2] | (src_row[x * 2 + 1] << 8);
                std::uint8_t r = static_cast<std::uint8_t>(((pixel & info.red_mask) >> info.red_shift) << info.red_scale);
                std::uint8_t g = static_cast<std::uint8_t>(((pixel & info.green_mask) >> info.green_shift) << info.green_scale);
                std::uint8_t b = static_cast<std::uint8_t>(((pixel & info.blue_mask) >> info.blue_shift) << info.blue_scale);
                row_buffer[x * 4 + 0] = r;
                row_buffer[x * 4 + 1] = g;
                row_buffer[x * 4 + 2] = b;
                row_buffer[x * 4 + 3] = 0xFF;
            }
            surf.write_pixels(0, y, info.width * 4, row_buffer.data());
        } else if (info.bits_per_pixel == 24) {
            // 24-bit BGR
            for (int x = 0; x < info.width; x++) {
                row_buffer[x * 4 + 0] = src_row[x * 3 + 2];  // R
                row_buffer[x * 4 + 1] = src_row[x * 3 + 1];  // G
                row_buffer[x * 4 + 2] = src_row[x * 3 + 0];  // B
                row_buffer[x * 4 + 3] = 0xFF;
            }
            surf.write_pixels(0, y, info.width * 4, row_buffer.data());
        } else if (info.bits_per_pixel == 32) {
            // 32-bit BGRA
            for (int x = 0; x < info.width; x++) {
                row_buffer[x * 4 + 0] = src_row[x * 4 + 2];  // R
                row_buffer[x * 4 + 1] = src_row[x * 4 + 1];  // G
                row_buffer[x * 4 + 2] = src_row[x * 4 + 0];  // B
                row_buffer[x * 4 + 3] = info.alpha_mask ? src_row[x * 4 + 3] : 0xFF;
            }
            surf.write_pixels(0, y, info.width * 4, row_buffer.data());
        }
    }

    return decode_result::success();
}

} // namespace onyx_image
