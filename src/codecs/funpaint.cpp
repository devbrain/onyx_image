#include <onyx_image/codecs/funpaint.hpp>
#include "c64_common.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace onyx_image {

namespace {

// FunPaint signature
constexpr char FUNPAINT_SIGNATURE[] = "FUNPAINT (MT) ";
constexpr std::size_t SIGNATURE_OFFSET = 2;
constexpr std::size_t SIGNATURE_LEN = 14;

// FunPaint unpacked size
constexpr std::size_t FUNPAINT_UNPACKED_SIZE = 33694;

// Maximum compression ratio (guard against decompression bombs)
constexpr std::size_t MAX_COMPRESSION_RATIO = 1000;

// FLI dimensions
constexpr int FLI_BUG_CHARACTERS = 3;
constexpr int HEIGHT = 200;

// IFLI offsets (from DecodeFunUnpacked in RECOIL)
constexpr std::size_t BITMAP1_OFFSET = 0x2012;      // 8210
constexpr std::size_t BITMAP2_OFFSET = 0x63fa;      // 25594
constexpr std::size_t VIDEO_MATRIX1_OFFSET = 0x12;  // 18
constexpr std::size_t VIDEO_MATRIX2_OFFSET = 0x43fa; // 17402
constexpr std::size_t COLOR_OFFSET = 0x4012;        // 16402

// Compression offsets
constexpr std::size_t COMPRESSION_FLAG_OFFSET = 16;
constexpr std::size_t ESCAPE_BYTE_OFFSET = 17;
constexpr std::size_t COMPRESSED_DATA_OFFSET = 18;

// Decompress DRP RLE format
bool decompress_drp(std::span<const std::uint8_t> data,
                    std::size_t start_offset,
                    std::uint8_t escape,
                    std::vector<std::uint8_t>& output,
                    std::size_t output_size) {
    // Guard against decompression bombs
    if (output_size > data.size() * MAX_COMPRESSION_RATIO) {
        return false;
    }

    output.clear();
    output.resize(output_size);

    // Copy the header (first 18 bytes) from input
    std::size_t header_size = std::min(start_offset, output_size);
    for (std::size_t i = 0; i < header_size; ++i) {
        output[i] = data[i];
    }

    std::size_t in_pos = start_offset;
    std::size_t out_pos = start_offset;

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

// Get multicolor pixel value for FLI mode
// In FLI mode, video matrix changes per scanline group (y & 7)
int get_fli_multicolor(const std::uint8_t* data,
                       std::size_t bitmap_offset,
                       std::size_t video_matrix_offset,
                       std::size_t color_offset,
                       int background,
                       int x,
                       int y,
                       int left_skip) {
    // Apply left skip (for interlaced frames)
    x += left_skip;
    if (x < 0) {
        return background;
    }

    // Calculate character cell position
    // Skip first FLI_BUG_CHARACTERS columns
    int char_col = x / 8;
    int char_row = y / 8;
    std::size_t char_offset = static_cast<std::size_t>(char_row * 40 + char_col);

    // Get bitmap byte - each char cell has 8 bytes, one per scanline
    int row_in_char = y % 8;
    std::size_t bitmap_byte_offset = bitmap_offset + char_offset * 8 + static_cast<std::size_t>(row_in_char);
    std::uint8_t bitmap_byte = data[bitmap_byte_offset];

    // Get 2-bit color selector for this pixel pair
    // Bits: 7-6, 5-4, 3-2, 1-0 map to pixel pairs 0-3
    int bit_pos = 6 - ((x % 8) & 6);  // Same as (~x & 6)
    int color_selector = (bitmap_byte >> bit_pos) & 0x03;

    // FLI mode: video matrix offset changes with scanline
    // videoMatrixOffset + ((y & 7) << 10) means 8 banks of 1024 bytes each
    std::size_t video_offset = video_matrix_offset + (static_cast<std::size_t>(y & 7) << 10) + char_offset;

    switch (color_selector) {
        case 0:
            return background;
        case 1:
            return (data[video_offset] >> 4) & 0x0f;
        case 2:
            return data[video_offset] & 0x0f;
        case 3:
            return data[color_offset + char_offset] & 0x0f;
        default:
            return background;
    }
}

// Decode one FLI frame
void decode_fli_frame(const std::uint8_t* data,
                      std::size_t bitmap_offset,
                      std::size_t video_matrix_offset,
                      std::size_t color_offset,
                      int background,
                      int left_skip,
                      std::vector<std::uint32_t>& pixels,
                      std::size_t pixels_offset) {
    // Adjust offsets to skip FLI bug characters
    bitmap_offset += FLI_BUG_CHARACTERS * 8;
    video_matrix_offset += FLI_BUG_CHARACTERS;
    color_offset += FLI_BUG_CHARACTERS;

    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < c64::FLI_WIDTH; ++x) {
            int color_index = get_fli_multicolor(
                data, bitmap_offset, video_matrix_offset, color_offset,
                background, x, y, left_skip);
            pixels[pixels_offset + static_cast<std::size_t>(y * c64::FLI_WIDTH + x)] =
                c64::PALETTE[color_index & 0x0f];
        }
    }
}

// Apply blend between two frames (average RGB values)
void apply_blend(std::vector<std::uint32_t>& pixels) {
    std::size_t frame_size = static_cast<std::size_t>(c64::FLI_WIDTH * HEIGHT);
    for (std::size_t i = 0; i < frame_size; ++i) {
        pixels[i] = c64::blend_rgb(pixels[i], pixels[frame_size + i]);
    }
}

}  // namespace

bool funpaint_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    // Minimum size: header + signature
    if (data.size() < SIGNATURE_OFFSET + SIGNATURE_LEN) {
        return false;
    }

    // Check for "FUNPAINT (MT) " signature at offset 2
    if (std::memcmp(data.data() + SIGNATURE_OFFSET, FUNPAINT_SIGNATURE, SIGNATURE_LEN) != 0) {
        return false;
    }

    return true;
}

decode_result funpaint_decoder::decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) {
    if (data.empty()) {
        return decode_result::failure(decode_error::truncated_data, "FunPaint file is empty");
    }

    // Check signature
    if (data.size() < SIGNATURE_OFFSET + SIGNATURE_LEN ||
        std::memcmp(data.data() + SIGNATURE_OFFSET, FUNPAINT_SIGNATURE, SIGNATURE_LEN) != 0) {
        return decode_result::failure(decode_error::invalid_format,
            "Missing FunPaint signature");
    }

    // Check dimension limits
    const int max_w = options.max_width > 0 ? options.max_width : 16384;
    const int max_h = options.max_height > 0 ? options.max_height : 16384;

    if (c64::FLI_WIDTH > max_w || HEIGHT > max_h) {
        return decode_result::failure(decode_error::dimensions_exceeded,
            "Image dimensions exceed limits");
    }

    // Determine if compressed and decompress if needed
    const std::uint8_t* source_data = data.data();
    std::vector<std::uint8_t> decompressed;

    if (data.size() < COMPRESSED_DATA_OFFSET) {
        return decode_result::failure(decode_error::truncated_data,
            "FunPaint file too small: expected at least 18 bytes");
    }

    std::uint8_t compression_flag = data[COMPRESSION_FLAG_OFFSET];

    if (compression_flag != 0) {
        // Compressed file - decompress using DRP RLE
        std::uint8_t escape = data[ESCAPE_BYTE_OFFSET];
        if (!decompress_drp(data, COMPRESSED_DATA_OFFSET, escape,
                           decompressed, FUNPAINT_UNPACKED_SIZE)) {
            return decode_result::failure(decode_error::truncated_data,
                "Failed to decompress FunPaint data");
        }
        source_data = decompressed.data();
    } else {
        // Uncompressed file - verify size
        if (data.size() != FUNPAINT_UNPACKED_SIZE) {
            return decode_result::failure(decode_error::invalid_format,
                "Invalid uncompressed FunPaint size");
        }
    }

    // Allocate surface (RGB output)
    if (!surf.set_size(c64::FLI_WIDTH, HEIGHT, pixel_format::rgb888)) {
        return decode_result::failure(decode_error::internal_error,
            "Failed to allocate surface");
    }

    // Allocate pixel buffers for two frames
    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(c64::FLI_WIDTH * HEIGHT * 2));

    // Decode first frame (left_skip = 0)
    decode_fli_frame(source_data, BITMAP1_OFFSET, VIDEO_MATRIX1_OFFSET,
                     COLOR_OFFSET, 0, 0, pixels, 0);

    // Decode second frame (left_skip = -1 for interlacing)
    decode_fli_frame(source_data, BITMAP2_OFFSET, VIDEO_MATRIX2_OFFSET,
                     COLOR_OFFSET, 0, -1, pixels,
                     static_cast<std::size_t>(c64::FLI_WIDTH * HEIGHT));

    // Blend frames
    apply_blend(pixels);

    // Write blended pixels to surface
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < c64::FLI_WIDTH; ++x) {
            std::uint32_t rgb = pixels[static_cast<std::size_t>(y * c64::FLI_WIDTH + x)];
            c64::write_rgb_pixel(surf, x, y, rgb);
        }
    }

    return decode_result::success();
}

}  // namespace onyx_image
