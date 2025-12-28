#include <onyx_image/codecs/koala.hpp>
#include "c64_common.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace onyx_image {

namespace {

// Standard Koala file sizes
constexpr std::size_t KOALA_SIZE_WITH_ADDR = 10003;     // With 2-byte load address
constexpr std::size_t KOALA_SIZE_WITHOUT_ADDR = 10001;  // Without load address
constexpr std::size_t KOALA_SIZE_OCP = 10018;           // OCP Art Studio variant

// GG (GodotGames) RLE escape byte
constexpr std::uint8_t GG_RLE_ESCAPE = 0xfe;

// Maximum compression ratio (guard against decompression bombs)
constexpr std::size_t MAX_COMPRESSION_RATIO = 1000;

// Decompress GG (GodotGames) RLE format
// Returns true on success, false on error
bool decompress_gg(std::span<const std::uint8_t> data,
                   std::size_t offset,
                   std::vector<std::uint8_t>& output,
                   std::size_t output_size) {
    // Guard against decompression bombs
    if (output_size > data.size() * MAX_COMPRESSION_RATIO) {
        return false;
    }

    output.clear();
    output.reserve(output_size);

    std::size_t pos = offset;
    while (output.size() < output_size && pos < data.size()) {
        std::uint8_t byte = data[pos++];

        if (byte == GG_RLE_ESCAPE) {
            // RLE: next byte is value, then count
            if (pos + 1 >= data.size()) {
                return false;  // Truncated
            }
            std::uint8_t value = data[pos++];
            std::uint8_t count = data[pos++];

            // Validate count won't exceed output size
            std::size_t remaining = output_size - output.size();
            std::size_t to_write = std::min(static_cast<std::size_t>(count), remaining);

            for (std::size_t i = 0; i < to_write; ++i) {
                output.push_back(value);
            }
        } else {
            // Literal byte
            output.push_back(byte);
        }
    }

    return output.size() == output_size;
}

// Check if data looks like uncompressed Koala
bool is_uncompressed_koala(std::span<const std::uint8_t> data) {
    // Standard sizes
    if (data.size() == KOALA_SIZE_WITHOUT_ADDR ||
        data.size() == KOALA_SIZE_WITH_ADDR ||
        data.size() == 10006 ||  // RPM variant
        data.size() == KOALA_SIZE_OCP) {
        return true;
    }
    return false;
}

// Check if data looks like GG (GodotGames) compressed Koala
bool is_gg_koala(std::span<const std::uint8_t> data) {
    // GG files have a 2-byte load address followed by RLE data
    // They are typically smaller than uncompressed Koala
    if (data.size() < 100 || data.size() >= KOALA_SIZE_WITHOUT_ADDR) {
        return false;
    }

    // Check for load address - must be a valid C64 address
    if (data.size() >= 2) {
        std::uint16_t load_addr = data[0] | (data[1] << 8);
        // Common Koala load addresses
        if (load_addr != 0x6000 && load_addr != 0x4000 &&
            load_addr != 0x2000 && load_addr != 0x5c00) {
            return false;
        }

        // Check for RLE escape byte anywhere in the data
        for (std::size_t i = 2; i < data.size(); ++i) {
            if (data[i] == GG_RLE_ESCAPE) {
                return true;  // Found RLE escape, likely compressed
            }
        }
    }

    return false;
}

}  // namespace

bool koala_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    // Check for uncompressed Koala by size
    if (is_uncompressed_koala(data)) {
        return true;
    }

    // Check for GG compressed format
    if (is_gg_koala(data)) {
        return true;
    }

    return false;
}

decode_result koala_decoder::decode(std::span<const std::uint8_t> data,
                                     surface& surf,
                                     const decode_options& options) {
    if (data.empty()) {
        return decode_result::failure(decode_error::truncated_data, "Koala file is empty");
    }

    // Determine offsets based on file format
    std::size_t bitmap_offset;
    std::size_t screen_offset;
    std::size_t color_offset;
    std::size_t background_offset;

    const std::uint8_t* source_data = data.data();
    std::vector<std::uint8_t> decompressed;

    if (is_gg_koala(data)) {
        // GG compressed format: decompress first
        if (!decompress_gg(data, 2, decompressed, KOALA_SIZE_WITHOUT_ADDR)) {
            return decode_result::failure(decode_error::truncated_data,
                "Failed to decompress GG Koala data");
        }
        source_data = decompressed.data();

        // Decompressed data has no load address
        bitmap_offset = 0;
        screen_offset = c64::BITMAP_SIZE;
        color_offset = c64::BITMAP_SIZE + c64::SCREEN_RAM_SIZE;
        background_offset = c64::BITMAP_SIZE + c64::SCREEN_RAM_SIZE + c64::COLOR_RAM_SIZE;
    } else if (data.size() == KOALA_SIZE_WITHOUT_ADDR) {
        // 10001 bytes: no load address
        bitmap_offset = 0;
        screen_offset = c64::BITMAP_SIZE;
        color_offset = c64::BITMAP_SIZE + c64::SCREEN_RAM_SIZE;
        background_offset = c64::BITMAP_SIZE + c64::SCREEN_RAM_SIZE + c64::COLOR_RAM_SIZE;
    } else if (data.size() == KOALA_SIZE_WITH_ADDR || data.size() == 10006) {
        // 10003 or 10006 bytes: with 2-byte load address
        bitmap_offset = 2;
        screen_offset = 2 + c64::BITMAP_SIZE;
        color_offset = 2 + c64::BITMAP_SIZE + c64::SCREEN_RAM_SIZE;
        background_offset = 2 + c64::BITMAP_SIZE + c64::SCREEN_RAM_SIZE + c64::COLOR_RAM_SIZE;
    } else if (data.size() == KOALA_SIZE_OCP) {
        // OCP Art Studio format (10018 bytes)
        bitmap_offset = 2;
        screen_offset = 2 + c64::BITMAP_SIZE;
        color_offset = 2 + c64::BITMAP_SIZE + 8 + c64::SCREEN_RAM_SIZE;
        background_offset = color_offset - 1;
    } else {
        return decode_result::failure(decode_error::invalid_format,
            "Unrecognized Koala file size");
    }

    // Validate we have enough data for all regions
    std::size_t available_size = decompressed.empty() ? data.size() : decompressed.size();
    std::size_t required_size = background_offset + 1;
    if (available_size < required_size) {
        return decode_result::failure(decode_error::truncated_data,
            "Koala data truncated: incomplete image data");
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

    // Decode the image using shared C64 multicolor decoder
    const std::uint8_t* bitmap = source_data + bitmap_offset;
    const std::uint8_t* screen_ram = source_data + screen_offset;
    const std::uint8_t* color_ram = source_data + color_offset;
    std::uint8_t background = source_data[background_offset];

    c64::decode_multicolor(bitmap, screen_ram, color_ram, background, surf);

    return decode_result::success();
}

}  // namespace onyx_image
