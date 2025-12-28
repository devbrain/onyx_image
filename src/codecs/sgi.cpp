#include <onyx_image/codecs/sgi.hpp>
#include "byte_io.hpp"
#include "decode_helpers.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace onyx_image {

namespace {

// SGI constants
constexpr std::uint16_t SGI_MAGIC = 474;
constexpr std::uint8_t SGI_STORAGE_VERBATIM = 0;
constexpr std::uint8_t SGI_STORAGE_RLE = 1;
constexpr std::size_t SGI_HEADER_SIZE = 512;

struct sgi_info {
    int width = 0;
    int height = 0;
    int channels = 0;
    int bpc = 0;           // Bytes per channel (1 or 2)
    std::uint8_t storage = 0;
    std::uint32_t colormap = 0;
};

bool parse_header(std::span<const std::uint8_t> data, sgi_info& info) {
    if (data.size() < SGI_HEADER_SIZE) {
        return false;
    }

    const std::uint8_t* ptr = data.data();

    std::uint16_t magic = read_be16(ptr);
    if (magic != SGI_MAGIC) {
        return false;
    }

    info.storage = ptr[2];
    info.bpc = ptr[3];
    // dimension at offset 4 (not used directly)
    info.width = static_cast<int>(read_be16(ptr + 6));
    info.height = static_cast<int>(read_be16(ptr + 8));
    info.channels = static_cast<int>(read_be16(ptr + 10));
    // pixmin at offset 12, pixmax at offset 16 (not used)
    info.colormap = read_be32(ptr + 104);

    return true;
}

// Decode RLE scanline (BPC=1)
bool decode_rle_scanline_8(
    const std::uint8_t* src, std::size_t src_len,
    std::uint8_t* dest, std::size_t width)
{
    std::size_t src_pos = 0;
    std::size_t dest_pos = 0;

    while (src_pos < src_len && dest_pos < width) {
        std::uint8_t ctrl = src[src_pos++];
        std::size_t count = ctrl & 0x7F;

        if (count == 0) {
            // End of scanline
            break;
        }

        if (ctrl & 0x80) {
            // Literal run: copy 'count' bytes
            if (src_pos + count > src_len || dest_pos + count > width) {
                return false;
            }
            std::memcpy(dest + dest_pos, src + src_pos, count);
            src_pos += count;
            dest_pos += count;
        } else {
            // Repeat run: repeat next byte 'count' times
            if (src_pos >= src_len || dest_pos + count > width) {
                return false;
            }
            std::uint8_t value = src[src_pos++];
            std::memset(dest + dest_pos, value, count);
            dest_pos += count;
        }
    }

    // Pad remainder with zeros if needed
    while (dest_pos < width) {
        dest[dest_pos++] = 0;
    }

    return true;
}

// Decode RLE scanline (BPC=2)
bool decode_rle_scanline_16(
    const std::uint8_t* src, std::size_t src_len,
    std::uint16_t* dest, std::size_t width)
{
    std::size_t src_pos = 0;
    std::size_t dest_pos = 0;

    while (src_pos + 1 < src_len && dest_pos < width) {
        std::uint16_t ctrl = read_be16(src + src_pos);
        src_pos += 2;
        std::size_t count = ctrl & 0x7F;

        if (count == 0) {
            break;
        }

        if (ctrl & 0x80) {
            // Literal run
            if (src_pos + count * 2 > src_len || dest_pos + count > width) {
                return false;
            }
            for (std::size_t i = 0; i < count; i++) {
                dest[dest_pos++] = read_be16(src + src_pos);
                src_pos += 2;
            }
        } else {
            // Repeat run
            if (src_pos + 1 >= src_len || dest_pos + count > width) {
                return false;
            }
            std::uint16_t value = read_be16(src + src_pos);
            src_pos += 2;
            for (std::size_t i = 0; i < count; i++) {
                dest[dest_pos++] = value;
            }
        }
    }

    while (dest_pos < width) {
        dest[dest_pos++] = 0;
    }

    return true;
}

} // namespace

bool sgi_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    if (data.size() < 2) {
        return false;
    }
    // Check big-endian magic number: 474 = 0x01DA
    return data[0] == 0x01 && data[1] == 0xDA;
}

decode_result sgi_decoder::decode(std::span<const std::uint8_t> data,
                                   surface& surf,
                                   const decode_options& options) {
    if (!sniff(data)) {
        return decode_result::failure(decode_error::invalid_format, "Not a valid SGI file");
    }

    sgi_info info;
    if (!parse_header(data, info)) {
        return decode_result::failure(decode_error::invalid_format, "Failed to parse SGI header");
    }

    if (info.width <= 0 || info.height <= 0) {
        return decode_result::failure(decode_error::invalid_format, "Invalid image dimensions");
    }

    if (info.bpc != 1 && info.bpc != 2) {
        return decode_result::failure(decode_error::unsupported_bit_depth,
            "Unsupported bytes per channel: " + std::to_string(info.bpc));
    }

    if (info.channels < 1 || info.channels > 4) {
        return decode_result::failure(decode_error::invalid_format,
            "Unsupported number of channels: " + std::to_string(info.channels));
    }

    if (info.colormap != 0) {
        return decode_result::failure(decode_error::unsupported_encoding,
            "Unsupported colormap type: " + std::to_string(info.colormap));
    }

    // Check dimension limits
    auto result = validate_dimensions(info.width, info.height, options);
    if (!result) return result;

    // Determine output format
    pixel_format out_format;
    if (info.channels == 1) {
        // Grayscale - output as RGB
        out_format = pixel_format::rgb888;
    } else if (info.channels == 2) {
        // Grayscale + alpha - output as RGBA
        out_format = pixel_format::rgba8888;
    } else if (info.channels == 3) {
        out_format = pixel_format::rgb888;
    } else {
        out_format = pixel_format::rgba8888;
    }

    if (!surf.set_size(info.width, info.height, out_format)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    const std::size_t width = static_cast<std::size_t>(info.width);
    const std::size_t height = static_cast<std::size_t>(info.height);
    const std::size_t channels = static_cast<std::size_t>(info.channels);

    // Allocate scanline buffer
    std::vector<std::uint8_t> scanline(width);
    std::vector<std::uint16_t> scanline16(width);

    // Allocate row buffer for output
    std::size_t out_bpp = (out_format == pixel_format::rgba8888) ? 4 : 3;
    std::vector<std::uint8_t> row_buffer(width * out_bpp);

    if (info.storage == SGI_STORAGE_RLE) {
        // RLE compressed
        // Read offset and length tables
        std::size_t table_entries = height * channels;
        std::size_t tables_size = table_entries * 4 * 2;  // Two tables of 4-byte entries

        if (data.size() < SGI_HEADER_SIZE + tables_size) {
            return decode_result::failure(decode_error::truncated_data,
                "SGI data truncated: incomplete RLE offset tables");
        }

        const std::uint8_t* start_table = data.data() + SGI_HEADER_SIZE;
        const std::uint8_t* len_table = start_table + table_entries * 4;

        // Decode each row
        for (std::size_t y = 0; y < height; y++) {
            // SGI images are stored bottom-up, flip to top-down
            int dest_y = static_cast<int>(height - 1 - y);

            // Initialize row buffer
            if (out_format == pixel_format::rgba8888) {
                // Set alpha to 255 by default
                for (std::size_t x = 0; x < width; x++) {
                    row_buffer[x * 4 + 3] = 255;
                }
            }

            // Decode each channel
            for (std::size_t c = 0; c < channels; c++) {
                std::size_t table_idx = y + c * height;
                std::uint32_t offset = read_be32(start_table + table_idx * 4);
                std::uint32_t length = read_be32(len_table + table_idx * 4);

                if (offset + length > data.size()) {
                    return decode_result::failure(decode_error::truncated_data,
                        "SGI data truncated: RLE data exceeds file size");
                }

                const std::uint8_t* rle_data = data.data() + offset;

                if (info.bpc == 1) {
                    if (!decode_rle_scanline_8(rle_data, length, scanline.data(), width)) {
                        return decode_result::failure(decode_error::invalid_format,
                            "SGI RLE decode failed: invalid compressed data");
                    }

                    // Copy to row buffer
                    if (info.channels == 1) {
                        // Grayscale to RGB
                        for (std::size_t x = 0; x < width; x++) {
                            row_buffer[x * 3 + 0] = scanline[x];
                            row_buffer[x * 3 + 1] = scanline[x];
                            row_buffer[x * 3 + 2] = scanline[x];
                        }
                    } else if (info.channels == 2) {
                        // Grayscale + alpha to RGBA
                        if (c == 0) {
                            for (std::size_t x = 0; x < width; x++) {
                                row_buffer[x * 4 + 0] = scanline[x];
                                row_buffer[x * 4 + 1] = scanline[x];
                                row_buffer[x * 4 + 2] = scanline[x];
                            }
                        } else {
                            for (std::size_t x = 0; x < width; x++) {
                                row_buffer[x * 4 + 3] = scanline[x];
                            }
                        }
                    } else if (info.channels == 3) {
                        // RGB
                        for (std::size_t x = 0; x < width; x++) {
                            row_buffer[x * 3 + c] = scanline[x];
                        }
                    } else {
                        // RGBA
                        for (std::size_t x = 0; x < width; x++) {
                            row_buffer[x * 4 + c] = scanline[x];
                        }
                    }
                } else {
                    // BPC=2: 16-bit samples, scale to 8-bit
                    if (!decode_rle_scanline_16(rle_data, length, scanline16.data(), width)) {
                        return decode_result::failure(decode_error::invalid_format,
                            "SGI RLE decode failed: invalid 16-bit compressed data");
                    }

                    // Scale 16-bit to 8-bit and copy
                    if (info.channels == 1) {
                        for (std::size_t x = 0; x < width; x++) {
                            std::uint8_t val = static_cast<std::uint8_t>(scanline16[x] >> 8);
                            row_buffer[x * 3 + 0] = val;
                            row_buffer[x * 3 + 1] = val;
                            row_buffer[x * 3 + 2] = val;
                        }
                    } else if (info.channels == 2) {
                        if (c == 0) {
                            for (std::size_t x = 0; x < width; x++) {
                                std::uint8_t val = static_cast<std::uint8_t>(scanline16[x] >> 8);
                                row_buffer[x * 4 + 0] = val;
                                row_buffer[x * 4 + 1] = val;
                                row_buffer[x * 4 + 2] = val;
                            }
                        } else {
                            for (std::size_t x = 0; x < width; x++) {
                                row_buffer[x * 4 + 3] = static_cast<std::uint8_t>(scanline16[x] >> 8);
                            }
                        }
                    } else if (info.channels == 3) {
                        for (std::size_t x = 0; x < width; x++) {
                            row_buffer[x * 3 + c] = static_cast<std::uint8_t>(scanline16[x] >> 8);
                        }
                    } else {
                        for (std::size_t x = 0; x < width; x++) {
                            row_buffer[x * 4 + c] = static_cast<std::uint8_t>(scanline16[x] >> 8);
                        }
                    }
                }
            }

            surf.write_pixels(0, dest_y, static_cast<int>(row_buffer.size()), row_buffer.data());
        }
    } else {
        // Uncompressed (VERBATIM)
        std::size_t scanline_size = width * static_cast<std::size_t>(info.bpc);
        std::size_t channel_size = scanline_size * height;
        std::size_t expected_size = SGI_HEADER_SIZE + channel_size * channels;

        if (data.size() < expected_size) {
            return decode_result::failure(decode_error::truncated_data,
                "SGI data truncated: incomplete image data");
        }

        const std::uint8_t* pixel_data = data.data() + SGI_HEADER_SIZE;

        for (std::size_t y = 0; y < height; y++) {
            int dest_y = static_cast<int>(height - 1 - y);

            if (out_format == pixel_format::rgba8888) {
                for (std::size_t x = 0; x < width; x++) {
                    row_buffer[x * 4 + 3] = 255;
                }
            }

            for (std::size_t c = 0; c < channels; c++) {
                const std::uint8_t* src_row = pixel_data + c * channel_size + y * scanline_size;

                if (info.bpc == 1) {
                    if (info.channels == 1) {
                        for (std::size_t x = 0; x < width; x++) {
                            row_buffer[x * 3 + 0] = src_row[x];
                            row_buffer[x * 3 + 1] = src_row[x];
                            row_buffer[x * 3 + 2] = src_row[x];
                        }
                    } else if (info.channels == 2) {
                        if (c == 0) {
                            for (std::size_t x = 0; x < width; x++) {
                                row_buffer[x * 4 + 0] = src_row[x];
                                row_buffer[x * 4 + 1] = src_row[x];
                                row_buffer[x * 4 + 2] = src_row[x];
                            }
                        } else {
                            for (std::size_t x = 0; x < width; x++) {
                                row_buffer[x * 4 + 3] = src_row[x];
                            }
                        }
                    } else if (info.channels == 3) {
                        for (std::size_t x = 0; x < width; x++) {
                            row_buffer[x * 3 + c] = src_row[x];
                        }
                    } else {
                        for (std::size_t x = 0; x < width; x++) {
                            row_buffer[x * 4 + c] = src_row[x];
                        }
                    }
                } else {
                    // 16-bit samples
                    if (info.channels == 1) {
                        for (std::size_t x = 0; x < width; x++) {
                            std::uint8_t val = src_row[x * 2];  // High byte (big-endian)
                            row_buffer[x * 3 + 0] = val;
                            row_buffer[x * 3 + 1] = val;
                            row_buffer[x * 3 + 2] = val;
                        }
                    } else if (info.channels == 2) {
                        if (c == 0) {
                            for (std::size_t x = 0; x < width; x++) {
                                std::uint8_t val = src_row[x * 2];
                                row_buffer[x * 4 + 0] = val;
                                row_buffer[x * 4 + 1] = val;
                                row_buffer[x * 4 + 2] = val;
                            }
                        } else {
                            for (std::size_t x = 0; x < width; x++) {
                                row_buffer[x * 4 + 3] = src_row[x * 2];
                            }
                        }
                    } else if (info.channels == 3) {
                        for (std::size_t x = 0; x < width; x++) {
                            row_buffer[x * 3 + c] = src_row[x * 2];
                        }
                    } else {
                        for (std::size_t x = 0; x < width; x++) {
                            row_buffer[x * 4 + c] = src_row[x * 2];
                        }
                    }
                }
            }

            surf.write_pixels(0, dest_y, static_cast<int>(row_buffer.size()), row_buffer.data());
        }
    }

    return decode_result::success();
}

} // namespace onyx_image
