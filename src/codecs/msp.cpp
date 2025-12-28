#include <onyx_image/codecs/msp.hpp>
#include <formats/msp/msp.hh>
#include "byte_io.hpp"

#include <cstring>
#include <vector>

namespace onyx_image {

namespace {

// Decode RLE-compressed scan line (version 2)
// Returns true only if the complete scanline was decoded
bool decode_rle_scanline(const std::uint8_t* src, std::size_t src_size,
                         std::uint8_t* dst, std::size_t dst_size) {
    std::size_t src_pos = 0;
    std::size_t dst_pos = 0;

    while (dst_pos < dst_size && src_pos < src_size) {
        std::uint8_t run_type = src[src_pos++];

        if (run_type == 0) {
            // RLE run: next byte is count, then value
            if (src_pos + 2 > src_size) return false;
            std::uint8_t run_count = src[src_pos++];
            std::uint8_t run_value = src[src_pos++];

            for (std::size_t i = 0; i < run_count && dst_pos < dst_size; ++i) {
                dst[dst_pos++] = run_value;
            }
        } else {
            // Literal run: run_type bytes follow
            std::size_t run_count = run_type;
            if (src_pos + run_count > src_size) return false;

            for (std::size_t i = 0; i < run_count && dst_pos < dst_size; ++i) {
                dst[dst_pos++] = src[src_pos++];
            }
        }
    }

    // Incomplete scanline is an error - don't silently pad with zeros
    if (dst_pos < dst_size) {
        return false;
    }

    return true;
}

// Convert 1-bit packed row to indexed8 format
void unpack_1bit_to_indexed8(const std::uint8_t* src, std::size_t src_bytes,
                              std::uint8_t* dst, std::size_t width) {
    std::size_t dst_pos = 0;
    for (std::size_t i = 0; i < src_bytes && dst_pos < width; ++i) {
        std::uint8_t byte = src[i];
        for (int bit = 7; bit >= 0 && dst_pos < width; --bit) {
            // MSP uses 1 = black, 0 = white (inverted from typical)
            dst[dst_pos++] = ((byte >> bit) & 1) ? 0 : 1;
        }
    }
}

} // namespace

bool msp_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    if (data.size() < 4) return false;

    std::uint16_t key1 = read_le16(data.data());
    std::uint16_t key2 = read_le16(data.data() + 2);

    return (key1 == formats::msp::MSP_V1_KEY1 && key2 == formats::msp::MSP_V1_KEY2) ||
           (key1 == formats::msp::MSP_V2_KEY1 && key2 == formats::msp::MSP_V2_KEY2);
}

decode_result msp_decoder::decode(std::span<const std::uint8_t> data,
                                   surface& surf,
                                   const decode_options& options) {
    if (data.size() < formats::msp::MSP_HEADER_SIZE) {
        return decode_result::failure(decode_error::truncated_data,
            "MSP file too small: expected at least 32 bytes");
    }

    // Parse header using DataScript
    formats::msp::msp_header hdr;
    try {
        const auto* ptr = data.data();
        const auto* end = ptr + data.size();
        hdr = formats::msp::msp_header::read(ptr, end);
    } catch (const std::exception& e) {
        return decode_result::failure(decode_error::invalid_format, e.what());
    }

    // Validate magic
    bool is_v1 = (hdr.key1 == formats::msp::MSP_V1_KEY1 && hdr.key2 == formats::msp::MSP_V1_KEY2);
    bool is_v2 = (hdr.key1 == formats::msp::MSP_V2_KEY1 && hdr.key2 == formats::msp::MSP_V2_KEY2);

    if (!is_v1 && !is_v2) {
        return decode_result::failure(decode_error::invalid_format, "Invalid MSP magic");
    }

    if (hdr.width == 0 || hdr.height == 0) {
        return decode_result::failure(decode_error::invalid_format, "Invalid MSP dimensions");
    }

    // Check dimension limits
    const int max_w = options.max_width > 0 ? options.max_width : 16384;
    const int max_h = options.max_height > 0 ? options.max_height : 16384;
    if (hdr.width > max_w || hdr.height > max_h) {
        return decode_result::failure(decode_error::dimensions_exceeded,
            "MSP image dimensions exceed limits");
    }

    // Bytes per row (1 bit per pixel, rounded up to byte boundary)
    std::size_t row_bytes = (static_cast<std::size_t>(hdr.width) + 7) / 8;

    // Allocate surface as indexed8 with 2-color palette
    if (!surf.set_size(hdr.width, hdr.height, pixel_format::indexed8)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    // Set up black/white palette (index 0 = black, index 1 = white)
    std::array<std::uint8_t, 6> palette = {
        0, 0, 0,       // Index 0: black (RGB)
        255, 255, 255  // Index 1: white (RGB)
    };
    surf.set_palette_size(2);
    surf.write_palette(0, std::span<const std::uint8_t>(palette.data(), palette.size()));

    std::vector<std::uint8_t> row_buffer(row_bytes);
    std::vector<std::uint8_t> pixel_row(hdr.width);

    if (is_v1) {
        // Version 1: uncompressed data immediately follows header
        std::size_t offset = formats::msp::MSP_HEADER_SIZE;
        std::size_t expected_size = formats::msp::MSP_HEADER_SIZE + row_bytes * hdr.height;

        if (data.size() < expected_size) {
            return decode_result::failure(decode_error::truncated_data,
                "MSP data truncated: incomplete image data");
        }

        for (std::size_t y = 0; y < hdr.height; ++y) {
            std::memcpy(row_buffer.data(), data.data() + offset, row_bytes);
            offset += row_bytes;

            unpack_1bit_to_indexed8(row_buffer.data(), row_bytes,
                                     pixel_row.data(), hdr.width);
            surf.write_pixels(0, static_cast<int>(y),
                              static_cast<int>(hdr.width), pixel_row.data());
        }
    } else {
        // Version 2: RLE compressed with scan-line map after header
        std::size_t scanline_map_size = static_cast<std::size_t>(hdr.height) * 2;
        std::size_t scanline_map_offset = formats::msp::MSP_HEADER_SIZE;

        if (data.size() < formats::msp::MSP_HEADER_SIZE + scanline_map_size) {
            return decode_result::failure(decode_error::truncated_data,
                "MSP data truncated: incomplete scanline map");
        }

        // Read scan-line map
        std::vector<std::uint16_t> scanline_sizes(hdr.height);
        for (std::size_t i = 0; i < hdr.height; ++i) {
            scanline_sizes[i] = read_le16(data.data() + scanline_map_offset + i * 2);
        }

        // Decode each scan line
        std::size_t data_offset = formats::msp::MSP_HEADER_SIZE + scanline_map_size;
        for (std::size_t y = 0; y < hdr.height; ++y) {
            std::size_t line_size = scanline_sizes[y];

            if (data_offset + line_size > data.size()) {
                return decode_result::failure(decode_error::truncated_data,
                    "MSP data truncated: incomplete scanline data");
            }

            if (!decode_rle_scanline(data.data() + data_offset, line_size,
                                      row_buffer.data(), row_bytes)) {
                return decode_result::failure(decode_error::unsupported_encoding,
                    "MSP RLE decompression failed");
            }
            data_offset += line_size;

            unpack_1bit_to_indexed8(row_buffer.data(), row_bytes,
                                     pixel_row.data(), hdr.width);
            surf.write_pixels(0, static_cast<int>(y),
                              static_cast<int>(hdr.width), pixel_row.data());
        }
    }

    return decode_result::success();
}

} // namespace onyx_image
