#include <onyx_image/codecs/runpaint.hpp>
#include "c64_common.hpp"

namespace onyx_image {

namespace {

// File sizes
constexpr std::size_t RPM_SIZE_STANDARD = 10003;  // Standard Run Paint
constexpr std::size_t RPM_SIZE_EXTENDED = 10006;  // Extended variant

// Offsets (with 2-byte load address prefix)
constexpr std::size_t BITMAP_OFFSET = 2;
constexpr std::size_t SCREEN_OFFSET = 2 + c64::BITMAP_SIZE;
constexpr std::size_t COLOR_OFFSET = 2 + c64::BITMAP_SIZE + c64::SCREEN_RAM_SIZE;
constexpr std::size_t BACKGROUND_OFFSET = 2 + c64::BITMAP_SIZE + c64::SCREEN_RAM_SIZE + c64::COLOR_RAM_SIZE;

// Minimum required data size
constexpr std::size_t MIN_REQUIRED_SIZE = BACKGROUND_OFFSET + 1;

}  // namespace

bool runpaint_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    // Check for valid Run Paint file sizes
    if (data.size() != RPM_SIZE_STANDARD && data.size() != RPM_SIZE_EXTENDED) {
        return false;
    }

    // Check for valid C64 load address
    if (data.size() >= 2) {
        std::uint16_t load_addr = data[0] | (data[1] << 8);
        // Common C64 multicolor load addresses
        if (load_addr != 0x6000 && load_addr != 0x4000 &&
            load_addr != 0x5c00 && load_addr != 0x2000) {
            return false;
        }
    }

    return true;
}

decode_result runpaint_decoder::decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) {
    if (data.empty()) {
        return decode_result::failure(decode_error::truncated_data, "Run Paint file is empty");
    }

    if (data.size() != RPM_SIZE_STANDARD && data.size() != RPM_SIZE_EXTENDED) {
        return decode_result::failure(decode_error::invalid_format,
            "Invalid Run Paint file size");
    }

    // Validate all required data regions are accessible
    if (data.size() < MIN_REQUIRED_SIZE) {
        return decode_result::failure(decode_error::truncated_data,
            "Run Paint data truncated: incomplete image data");
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
    const std::uint8_t* bitmap = data.data() + BITMAP_OFFSET;
    const std::uint8_t* screen_ram = data.data() + SCREEN_OFFSET;
    const std::uint8_t* color_ram = data.data() + COLOR_OFFSET;
    std::uint8_t background = data[BACKGROUND_OFFSET];

    c64::decode_multicolor(bitmap, screen_ram, color_ram, background, surf);

    return decode_result::success();
}

}  // namespace onyx_image
