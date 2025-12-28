#include <onyx_image/codecs/c64_doodle.hpp>
#include "c64_common.hpp"

#include <algorithm>
#include <vector>

namespace onyx_image {

namespace {

// JJ RLE escape byte (same as Koala GG format)
constexpr std::uint8_t JJ_RLE_ESCAPE = 0xfe;

// Maximum compression ratio (guard against decompression bombs)
constexpr std::size_t MAX_COMPRESSION_RATIO = 1000;

// Valid uncompressed Doodle file sizes
constexpr std::size_t DOODLE_SIZE_RUN_PAINT = 9026;
constexpr std::size_t DOODLE_SIZE_HIRES_EDITOR = 9217;
constexpr std::size_t DOODLE_SIZE_STANDARD = 9218;
constexpr std::size_t DOODLE_SIZE_EXTENDED = 9346;

// JJ decompressed sizes (9024 standard, 9216 extended variant)
constexpr std::size_t JJ_UNPACKED_SIZE = 9024;
constexpr std::size_t JJ_UNPACKED_SIZE_EXT = 9216;

// Decompress JJ RLE format (same as Koala GG)
// If max_output is 0, decompresses fully. Otherwise limits to max_output bytes.
bool decompress_jj(std::span<const std::uint8_t> data,
                   std::size_t offset,
                   std::vector<std::uint8_t>& output,
                   std::size_t max_output = 0) {
    // Guard against decompression bombs
    if (max_output > 0 && max_output > data.size() * MAX_COMPRESSION_RATIO) {
        return false;
    }

    output.clear();
    if (max_output > 0) {
        output.reserve(max_output);
    }

    std::size_t pos = offset;
    while (pos < data.size()) {
        if (max_output > 0 && output.size() >= max_output) {
            break;
        }

        std::uint8_t byte = data[pos++];

        if (byte == JJ_RLE_ESCAPE) {
            // RLE: next byte is value, then count
            if (pos + 1 >= data.size()) {
                return false;  // Truncated
            }
            std::uint8_t value = data[pos++];
            std::uint8_t count = data[pos++];

            // Validate count won't exceed output size
            std::size_t remaining = max_output > 0 ? max_output - output.size() : SIZE_MAX;
            std::size_t to_write = std::min(static_cast<std::size_t>(count), remaining);

            for (std::size_t i = 0; i < to_write; ++i) {
                output.push_back(value);
            }
        } else {
            // Literal byte
            output.push_back(byte);
        }
    }

    return max_output == 0 || output.size() == max_output;
}

// Check if data looks like uncompressed Doodle
bool is_uncompressed_doodle(std::size_t size) {
    return size == DOODLE_SIZE_RUN_PAINT ||
           size == DOODLE_SIZE_HIRES_EDITOR ||
           size == DOODLE_SIZE_STANDARD ||
           size == DOODLE_SIZE_EXTENDED;
}

// Check if data looks like JJ compressed Doodle
bool is_jj_doodle(std::span<const std::uint8_t> data) {
    // JJ files are smaller than uncompressed and contain RLE escape bytes
    if (data.size() < 100 || data.size() >= DOODLE_SIZE_RUN_PAINT) {
        return false;
    }

    // Try to decompress fully - JJ must decompress to 9024 or 9216 bytes
    // This distinguishes JJ from Koala GG (which decompresses to 10003)
    std::vector<std::uint8_t> decompressed;
    if (!decompress_jj(data, 2, decompressed)) {  // Decompress fully
        return false;
    }

    return decompressed.size() == JJ_UNPACKED_SIZE ||
           decompressed.size() == JJ_UNPACKED_SIZE_EXT;
}

}  // namespace

bool c64_doodle_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    // Check for uncompressed Doodle by size
    if (is_uncompressed_doodle(data.size())) {
        return true;
    }

    // Check for JJ compressed format
    if (is_jj_doodle(data)) {
        return true;
    }

    return false;
}

decode_result c64_doodle_decoder::decode(std::span<const std::uint8_t> data,
                                          surface& surf,
                                          const decode_options& options) {
    if (data.empty()) {
        return decode_result::failure(decode_error::truncated_data, "Doodle file is empty");
    }

    // Determine offsets based on file format
    std::size_t bitmap_offset;
    std::size_t video_matrix_offset;

    const std::uint8_t* source_data = data.data();
    std::vector<std::uint8_t> decompressed;

    if (is_jj_doodle(data)) {
        // JJ compressed format: decompress first
        if (!decompress_jj(data, 2, decompressed, JJ_UNPACKED_SIZE)) {
            return decode_result::failure(decode_error::truncated_data,
                "Failed to decompress JJ Doodle data");
        }
        source_data = decompressed.data();

        // JJ decompressed layout: video matrix at 0, bitmap at 0x400
        video_matrix_offset = 0;
        bitmap_offset = 0x400;  // 1024
    } else if (is_uncompressed_doodle(data.size())) {
        // Uncompressed Doodle: video matrix at 2, bitmap at 0x402
        video_matrix_offset = 2;
        bitmap_offset = 0x402;  // 1026
    } else {
        return decode_result::failure(decode_error::invalid_format,
            "Unrecognized Doodle file size");
    }

    // Validate we have enough data
    std::size_t required_size = bitmap_offset + c64::BITMAP_SIZE;
    std::size_t available_size = decompressed.empty() ? data.size() : decompressed.size();
    if (available_size < required_size) {
        return decode_result::failure(decode_error::truncated_data,
            "Doodle data truncated: incomplete image data");
    }

    // Check dimension limits
    const int max_w = options.max_width > 0 ? options.max_width : 16384;
    const int max_h = options.max_height > 0 ? options.max_height : 16384;

    if (c64::HIRES_WIDTH > max_w || c64::HIRES_HEIGHT > max_h) {
        return decode_result::failure(decode_error::dimensions_exceeded,
            "Image dimensions exceed limits");
    }

    // Allocate surface (RGB output)
    if (!surf.set_size(c64::HIRES_WIDTH, c64::HIRES_HEIGHT, pixel_format::rgb888)) {
        return decode_result::failure(decode_error::internal_error,
            "Failed to allocate surface");
    }

    // Decode the image using shared C64 hires decoder
    const std::uint8_t* bitmap = source_data + bitmap_offset;
    const std::uint8_t* video_matrix = source_data + video_matrix_offset;

    c64::decode_hires(bitmap, video_matrix, 0x10, surf);

    return decode_result::success();
}

}  // namespace onyx_image
