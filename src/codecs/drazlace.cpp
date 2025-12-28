#include <onyx_image/codecs/drazlace.hpp>
#include "c64_common.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace onyx_image {

namespace {

// Uncompressed DrazLace file size
constexpr std::size_t DRAZLACE_UNPACKED_SIZE = 18242;

// Maximum compression ratio (guard against decompression bombs)
constexpr std::size_t MAX_COMPRESSION_RATIO = 1000;

// DrazLace signature
constexpr char DRAZLACE_SIGNATURE[] = "DRAZLACE! 1.0";
constexpr std::size_t DRAZLACE_SIGNATURE_LEN = 13;

// Offsets in unpacked data
constexpr std::size_t COLOR_OFFSET = 2;           // 0x0002
constexpr std::size_t VIDEO_MATRIX_OFFSET = 0x402;  // 1026
constexpr std::size_t BITMAP1_OFFSET = 0x802;       // 2050
constexpr std::size_t BITMAP2_OFFSET = 0x2802;      // 10242
constexpr std::size_t BACKGROUND_OFFSET = 0x2742;   // 10050
constexpr std::size_t SHIFT_OFFSET = 0x2744;        // 10052

// Decompress DRP (DrazPaint) RLE format
// Format: escape byte at content[15], compressed data starts at offset 16
// RLE: escape, count, value
bool decompress_drp(std::span<const std::uint8_t> data,
                    std::vector<std::uint8_t>& output,
                    std::size_t output_size) {
    if (data.size() < 17) {
        return false;
    }

    // Guard against decompression bombs
    if (output_size > data.size() * MAX_COMPRESSION_RATIO) {
        return false;
    }

    std::uint8_t escape = data[15];
    output.clear();
    output.resize(output_size);

    // First 2 bytes of output are the load address (copied from compressed file)
    output[0] = data[0];
    output[1] = data[1];

    std::size_t in_pos = 16;
    std::size_t out_pos = 2;

    while (out_pos < output_size && in_pos < data.size()) {
        std::uint8_t byte = data[in_pos++];

        if (byte == escape) {
            if (in_pos >= data.size()) {
                return false;  // Truncated
            }
            std::uint8_t count = data[in_pos++];
            if (in_pos >= data.size()) {
                return false;  // Truncated
            }
            std::uint8_t value = data[in_pos++];

            // Validate count won't exceed output size
            std::size_t remaining = output_size - out_pos;
            std::size_t to_write = std::min(static_cast<std::size_t>(count), remaining);

            for (std::size_t i = 0; i < to_write; ++i) {
                output[out_pos++] = value;
            }
        } else {
            output[out_pos++] = byte;
        }
    }

    return out_pos == output_size;
}

// Get multicolor pixel value
int get_c64_multicolor(const std::uint8_t* content,
                       std::size_t bitmap_offset,
                       std::size_t video_matrix_offset,
                       std::size_t color_offset,
                       std::uint8_t background,
                       int x, int y, int left_skip) {
    x += left_skip;
    if (x < 0) {
        return background;
    }

    std::size_t char_offset = static_cast<std::size_t>((y / 8) * 40 + (x / 8));
    std::size_t row_in_char = static_cast<std::size_t>(y % 8);

    // Get 2-bit color selector
    // Bits are paired: 76, 54, 32, 10
    int bit_shift = 6 - ((x % 8) / 2) * 2;
    int color_sel = (content[bitmap_offset + char_offset * 8 + row_in_char] >> bit_shift) & 0x03;

    switch (color_sel) {
        case 0:
            return background;
        case 1:
            return (content[video_matrix_offset + char_offset] >> 4) & 0x0f;
        case 2:
            return content[video_matrix_offset + char_offset] & 0x0f;
        case 3:
            return content[color_offset + char_offset] & 0x0f;
        default:
            return background;
    }
}

// Decode one multicolor frame into a pixel buffer
void decode_c64_multicolor_frame(const std::uint8_t* content,
                                  std::size_t bitmap_offset,
                                  std::size_t video_matrix_offset,
                                  std::size_t color_offset,
                                  std::uint8_t background,
                                  int left_skip,
                                  std::vector<std::uint32_t>& pixels) {
    for (int y = 0; y < c64::MULTICOLOR_HEIGHT; ++y) {
        for (int x = 0; x < c64::MULTICOLOR_WIDTH; ++x) {
            int color_index = get_c64_multicolor(content, bitmap_offset, video_matrix_offset,
                                                  color_offset, background, x, y, left_skip);
            std::size_t idx = static_cast<std::size_t>(y * c64::MULTICOLOR_WIDTH + x);
            pixels[idx] = c64::PALETTE[static_cast<std::size_t>(color_index & 0x0f)];
        }
    }
}

// Apply interlace blending between two frames
void apply_blend(const std::vector<std::uint32_t>& frame1,
                 const std::vector<std::uint32_t>& frame2,
                 surface& surf) {
    for (int y = 0; y < c64::MULTICOLOR_HEIGHT; ++y) {
        for (int x = 0; x < c64::MULTICOLOR_WIDTH; ++x) {
            std::size_t idx = static_cast<std::size_t>(y * c64::MULTICOLOR_WIDTH + x);
            std::uint32_t blended = c64::blend_rgb(frame1[idx], frame2[idx]);
            c64::write_rgb_pixel(surf, x, y, blended);
        }
    }
}

// Check if data has DrazLace signature
bool has_drazlace_signature(std::span<const std::uint8_t> data) {
    if (data.size() < 2 + DRAZLACE_SIGNATURE_LEN) {
        return false;
    }
    return std::memcmp(data.data() + 2, DRAZLACE_SIGNATURE, DRAZLACE_SIGNATURE_LEN) == 0;
}

}  // namespace

bool drazlace_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    // Check for uncompressed DrazLace by size
    if (data.size() == DRAZLACE_UNPACKED_SIZE) {
        return true;
    }

    // Check for compressed DrazLace by signature
    if (has_drazlace_signature(data)) {
        return true;
    }

    return false;
}

decode_result drazlace_decoder::decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) {
    if (data.empty()) {
        return decode_result::failure(decode_error::truncated_data, "DrazLace file is empty");
    }

    const std::uint8_t* source_data = data.data();
    std::vector<std::uint8_t> decompressed;

    // Decompress if needed
    if (has_drazlace_signature(data)) {
        if (!decompress_drp(data, decompressed, DRAZLACE_UNPACKED_SIZE)) {
            return decode_result::failure(decode_error::truncated_data,
                "Failed to decompress DrazLace data");
        }
        source_data = decompressed.data();
    } else if (data.size() == DRAZLACE_UNPACKED_SIZE) {
        // Uncompressed
    } else {
        return decode_result::failure(decode_error::invalid_format,
            "Unrecognized DrazLace file format");
    }

    // Get shift value (0 or 1 for interlace mode)
    int shift = source_data[SHIFT_OFFSET];
    if (shift > 1) {
        return decode_result::failure(decode_error::invalid_format,
            "Invalid DrazLace shift value");
    }

    // Check dimension limits
    const int max_w = options.max_width > 0 ? options.max_width : 16384;
    const int max_h = options.max_height > 0 ? options.max_height : 16384;

    if (c64::MULTICOLOR_WIDTH > max_w || c64::MULTICOLOR_HEIGHT > max_h) {
        return decode_result::failure(decode_error::dimensions_exceeded,
            "Image dimensions exceed limits");
    }

    // Allocate surface (RGB output)
    if (!surf.set_size(c64::MULTICOLOR_WIDTH, c64::MULTICOLOR_HEIGHT, pixel_format::rgb888)) {
        return decode_result::failure(decode_error::internal_error,
            "Failed to allocate surface");
    }

    // Get background color
    std::uint8_t background = source_data[BACKGROUND_OFFSET];

    // Decode two frames
    std::vector<std::uint32_t> frame1(c64::MULTICOLOR_WIDTH * c64::MULTICOLOR_HEIGHT);
    std::vector<std::uint32_t> frame2(c64::MULTICOLOR_WIDTH * c64::MULTICOLOR_HEIGHT);

    // Frame 1: bitmap1 with video matrix, left_skip = 0
    decode_c64_multicolor_frame(source_data, BITMAP1_OFFSET, VIDEO_MATRIX_OFFSET,
                                 COLOR_OFFSET, background, 0, frame1);

    // Frame 2: bitmap2 with same video matrix, left_skip = -shift
    decode_c64_multicolor_frame(source_data, BITMAP2_OFFSET, VIDEO_MATRIX_OFFSET,
                                 COLOR_OFFSET, background, -shift, frame2);

    // Apply interlace blending
    apply_blend(frame1, frame2, surf);

    return decode_result::success();
}

}  // namespace onyx_image
