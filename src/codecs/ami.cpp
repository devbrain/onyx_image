#include <onyx_image/codecs/ami.hpp>
#include "c64_common.hpp"

#include <algorithm>
#include <vector>

namespace onyx_image {

namespace {

// AMI fixed escape byte for DRP RLE compression
constexpr std::uint8_t AMI_ESCAPE = 0xc2;

// Koala unpacked size
constexpr std::size_t KOALA_UNPACKED_SIZE = 10001;

// Maximum compression ratio (guard against decompression bombs)
constexpr std::size_t MAX_COMPRESSION_RATIO = 1000;

// Decompress DRP RLE format with fixed escape byte
bool decompress_ami(std::span<const std::uint8_t> data,
                    std::vector<std::uint8_t>& output) {
    if (data.size() < 3) {
        return false;
    }

    // Guard against decompression bombs
    if (KOALA_UNPACKED_SIZE > data.size() * MAX_COMPRESSION_RATIO) {
        return false;
    }

    output.clear();
    output.reserve(KOALA_UNPACKED_SIZE);

    std::size_t in_pos = 2;  // Skip 2-byte load address

    while (output.size() < KOALA_UNPACKED_SIZE && in_pos < data.size()) {
        std::uint8_t byte = data[in_pos++];

        if (byte == AMI_ESCAPE) {
            if (in_pos >= data.size()) {
                return false;  // Truncated
            }
            std::uint8_t count = data[in_pos++];
            if (in_pos >= data.size()) {
                return false;  // Truncated
            }
            std::uint8_t value = data[in_pos++];

            // Validate count won't exceed output size
            std::size_t remaining = KOALA_UNPACKED_SIZE - output.size();
            std::size_t to_write = std::min(static_cast<std::size_t>(count), remaining);

            for (std::size_t i = 0; i < to_write; ++i) {
                output.push_back(value);
            }
        } else {
            output.push_back(byte);
        }
    }

    return output.size() == KOALA_UNPACKED_SIZE;
}

}  // namespace

bool ami_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    // AMI files are DRP RLE compressed Koala images
    if (data.size() < 100 || data.size() >= KOALA_UNPACKED_SIZE) {
        return false;
    }

    // AMI files use load address 0x4000
    if (data.size() < 2) {
        return false;
    }
    std::uint16_t load_addr = data[0] | (data[1] << 8);
    if (load_addr != 0x4000) {
        return false;
    }

    // Look for the escape byte 0xc2 in the data
    for (std::size_t i = 2; i < data.size() && i < 500; ++i) {
        if (data[i] == AMI_ESCAPE) {
            return true;
        }
    }

    // Accept small files with 0x4000 load address
    return data.size() < 9000;
}

decode_result ami_decoder::decode(std::span<const std::uint8_t> data,
                                   surface& surf,
                                   const decode_options& options) {
    if (data.empty()) {
        return decode_result::failure(decode_error::truncated_data, "Amica Paint file is empty");
    }

    if (data.size() < 3) {
        return decode_result::failure(decode_error::truncated_data,
            "Amica Paint file too small: expected at least 3 bytes");
    }

    // Check dimension limits
    const int max_w = options.max_width > 0 ? options.max_width : 16384;
    const int max_h = options.max_height > 0 ? options.max_height : 16384;

    if (c64::MULTICOLOR_WIDTH > max_w || c64::MULTICOLOR_HEIGHT > max_h) {
        return decode_result::failure(decode_error::dimensions_exceeded,
            "Image dimensions exceed limits");
    }

    // Decompress the data
    std::vector<std::uint8_t> unpacked;
    if (!decompress_ami(data, unpacked)) {
        return decode_result::failure(decode_error::truncated_data,
            "Failed to decompress AMI data");
    }

    // Allocate surface (RGB output)
    if (!surf.set_size(c64::MULTICOLOR_WIDTH, c64::MULTICOLOR_HEIGHT, pixel_format::rgb888)) {
        return decode_result::failure(decode_error::internal_error,
            "Failed to allocate surface");
    }

    // Koala layout offsets (no load address in decompressed data)
    constexpr std::size_t bitmap_offset = 0;
    constexpr std::size_t screen_offset = c64::BITMAP_SIZE;
    constexpr std::size_t color_offset = c64::BITMAP_SIZE + c64::SCREEN_RAM_SIZE;
    constexpr std::size_t background_offset = c64::BITMAP_SIZE + c64::SCREEN_RAM_SIZE + c64::COLOR_RAM_SIZE;

    const std::uint8_t* bitmap = unpacked.data() + bitmap_offset;
    const std::uint8_t* screen_ram = unpacked.data() + screen_offset;
    const std::uint8_t* color_ram = unpacked.data() + color_offset;
    std::uint8_t background = unpacked[background_offset];

    c64::decode_multicolor(bitmap, screen_ram, color_ram, background, surf);

    return decode_result::success();
}

}  // namespace onyx_image
