#include <onyx_image/codecs/interpaint.hpp>
#include "c64_common.hpp"

namespace onyx_image {

namespace {

// File sizes
constexpr std::size_t IPH_SIZE_9002 = 9002;   // IPH hires variant
constexpr std::size_t IPH_SIZE_9003 = 9003;   // IPH hires variant (HPC)
constexpr std::size_t IPH_SIZE_9009 = 9009;   // IPH hires variant (AAS)
constexpr std::size_t IPT_SIZE = 10003;       // IPT multicolor

// Minimum required sizes
constexpr std::size_t IPH_MIN_SIZE = 2 + c64::BITMAP_SIZE + c64::SCREEN_RAM_SIZE;
constexpr std::size_t IPT_MIN_SIZE = 2 + c64::BITMAP_SIZE + c64::SCREEN_RAM_SIZE + c64::COLOR_RAM_SIZE + 1;

bool is_iph_size(std::size_t size) {
    return size == IPH_SIZE_9002 || size == IPH_SIZE_9003 || size == IPH_SIZE_9009;
}

}  // namespace

bool interpaint_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    // Check for IPH (hires) by size
    if (is_iph_size(data.size())) {
        return true;
    }

    // Check for IPT (multicolor) by size
    if (data.size() == IPT_SIZE) {
        return true;
    }

    return false;
}

decode_result interpaint_decoder::decode(std::span<const std::uint8_t> data,
                                          surface& surf,
                                          const decode_options& options) {
    if (data.empty()) {
        return decode_result::failure(decode_error::truncated_data, "InterPaint file is empty");
    }

    const int max_w = options.max_width > 0 ? options.max_width : 16384;
    const int max_h = options.max_height > 0 ? options.max_height : 16384;

    if (is_iph_size(data.size())) {
        // IPH: Hires format
        if (data.size() < IPH_MIN_SIZE) {
            return decode_result::failure(decode_error::truncated_data,
                "InterPaint hires data truncated: incomplete image data");
        }

        if (c64::HIRES_WIDTH > max_w || c64::HIRES_HEIGHT > max_h) {
            return decode_result::failure(decode_error::dimensions_exceeded,
                "Image dimensions exceed limits");
        }

        if (!surf.set_size(c64::HIRES_WIDTH, c64::HIRES_HEIGHT, pixel_format::rgb888)) {
            return decode_result::failure(decode_error::internal_error,
                "Failed to allocate surface");
        }

        // IPH offsets: bitmap at 2, video matrix at 0x1f42
        const std::uint8_t* bitmap = data.data() + 2;
        const std::uint8_t* video_matrix = data.data() + 0x1f42;

        c64::decode_hires(bitmap, video_matrix, 0x10, surf);

    } else if (data.size() == IPT_SIZE) {
        // IPT: Multicolor format
        if (data.size() < IPT_MIN_SIZE) {
            return decode_result::failure(decode_error::truncated_data,
                "InterPaint multicolor data truncated: incomplete image data");
        }

        if (c64::MULTICOLOR_WIDTH > max_w || c64::MULTICOLOR_HEIGHT > max_h) {
            return decode_result::failure(decode_error::dimensions_exceeded,
                "Image dimensions exceed limits");
        }

        if (!surf.set_size(c64::MULTICOLOR_WIDTH, c64::MULTICOLOR_HEIGHT, pixel_format::rgb888)) {
            return decode_result::failure(decode_error::internal_error,
                "Failed to allocate surface");
        }

        // IPT offsets (same as Koala with load address)
        constexpr std::size_t bitmap_offset = 2;
        constexpr std::size_t screen_offset = 2 + c64::BITMAP_SIZE;
        constexpr std::size_t color_offset = 2 + c64::BITMAP_SIZE + c64::SCREEN_RAM_SIZE;
        constexpr std::size_t background_offset = 2 + c64::BITMAP_SIZE + c64::SCREEN_RAM_SIZE + c64::COLOR_RAM_SIZE;

        const std::uint8_t* bitmap = data.data() + bitmap_offset;
        const std::uint8_t* screen_ram = data.data() + screen_offset;
        const std::uint8_t* color_ram = data.data() + color_offset;
        std::uint8_t background = data[background_offset];

        c64::decode_multicolor(bitmap, screen_ram, color_ram, background, surf);

    } else {
        return decode_result::failure(decode_error::invalid_format,
            "Invalid InterPaint file size");
    }

    return decode_result::success();
}

}  // namespace onyx_image
