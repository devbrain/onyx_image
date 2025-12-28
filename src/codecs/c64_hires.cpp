#include <onyx_image/codecs/c64_hires.hpp>
#include "c64_common.hpp"

namespace onyx_image {

namespace {

// File sizes
constexpr std::size_t HIRES_SIZE_BASIC = 8002;      // Bitmap only (load addr + 8000)
constexpr std::size_t HIRES_SIZE_8194 = 8194;       // GCD, MON variant
constexpr std::size_t HIRES_SIZE_WITH_COLORS = 9002;  // With video matrix
constexpr std::size_t HIRES_SIZE_HPC = 9003;        // HPC variant
constexpr std::size_t HIRES_SIZE_AAS = 9009;        // AAS, ART variant

// Offsets
constexpr std::size_t BITMAP_OFFSET = 2;            // After load address
constexpr std::size_t VIDEO_MATRIX_OFFSET = 2 + c64::BITMAP_SIZE;  // 8002

// Check if file size matches C64 hires format
bool is_hires_size(std::size_t size) {
    return size == HIRES_SIZE_BASIC ||
           size == HIRES_SIZE_8194 ||
           size == HIRES_SIZE_WITH_COLORS ||
           size == HIRES_SIZE_HPC ||
           size == HIRES_SIZE_AAS;
}

}  // namespace

bool c64_hires_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    // Check for valid C64 hires file sizes
    if (!is_hires_size(data.size())) {
        return false;
    }

    // Check for valid C64 load address
    if (data.size() >= 2) {
        std::uint16_t load_addr = data[0] | (data[1] << 8);
        // Common C64 hires load addresses
        if (load_addr != 0x2000 && load_addr != 0x4000 &&
            load_addr != 0x6000 && load_addr != 0xa000 &&
            load_addr != 0x5c00 && load_addr != 0x4100 &&
            load_addr != 0x3f40 && load_addr != 0x1c00 &&
            load_addr != 0x6c00) {
            return false;
        }
    }

    return true;
}

decode_result c64_hires_decoder::decode(std::span<const std::uint8_t> data,
                                         surface& surf,
                                         const decode_options& options) {
    if (data.empty()) {
        return decode_result::failure(decode_error::truncated_data, "C64 hires file is empty");
    }

    if (!is_hires_size(data.size())) {
        return decode_result::failure(decode_error::invalid_format,
            "Invalid C64 hires file size");
    }

    // Validate minimum data size for bitmap
    if (data.size() < BITMAP_OFFSET + c64::BITMAP_SIZE) {
        return decode_result::failure(decode_error::truncated_data,
            "C64 hires data truncated: incomplete bitmap data");
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

    // Determine if we have video matrix data
    const std::uint8_t* bitmap = data.data() + BITMAP_OFFSET;
    const std::uint8_t* video_matrix = nullptr;
    std::uint8_t fixed_colors = 0x10;  // Default: black background (0), white foreground (1)

    if (data.size() >= HIRES_SIZE_WITH_COLORS) {
        // Has video matrix at offset after bitmap
        if (data.size() >= VIDEO_MATRIX_OFFSET + c64::SCREEN_RAM_SIZE) {
            video_matrix = data.data() + VIDEO_MATRIX_OFFSET;
        }
    }

    // Decode the image using shared C64 hires decoder
    c64::decode_hires(bitmap, video_matrix, fixed_colors, surf);

    return decode_result::success();
}

}  // namespace onyx_image
