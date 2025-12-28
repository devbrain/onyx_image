#ifndef ONYX_IMAGE_TYPES_HPP_
#define ONYX_IMAGE_TYPES_HPP_

#include <onyx_image/onyx_image_export.h>

#include <cstdint>
#include <string>

namespace onyx_image {

// ============================================================================
// Pixel Formats
// ============================================================================

enum class pixel_format {
    indexed8,   // 8-bit indices, up to 256 colors
    rgb888,     // 24-bit, 8-bit RGB components, no alpha
    rgba8888    // 32-bit, 8-bit RGBA components
};

[[nodiscard]] constexpr std::size_t bytes_per_pixel(pixel_format fmt) noexcept {
    switch (fmt) {
        case pixel_format::indexed8: return 1;
        case pixel_format::rgb888:   return 3;
        case pixel_format::rgba8888: return 4;
    }
    return 0;
}

// ============================================================================
// Subrect Metadata (for multi-image containers)
// ============================================================================

struct image_rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

enum class subrect_kind {
    sprite,
    tile,
    frame
};

struct subrect {
    image_rect rect;
    subrect_kind kind = subrect_kind::sprite;
    std::uint32_t user_tag = 0;
};

// ============================================================================
// Decode Errors
// ============================================================================

enum class decode_error {
    none,
    invalid_format,
    unsupported_version,
    unsupported_encoding,
    unsupported_bit_depth,
    dimensions_exceeded,
    truncated_data,
    io_error,
    internal_error
};

[[nodiscard]] ONYX_IMAGE_EXPORT const char* to_string(decode_error err) noexcept;

// ============================================================================
// Decode Result
// ============================================================================

struct decode_result {
    bool ok = false;
    decode_error error = decode_error::none;
    std::string message;

    [[nodiscard]] static decode_result success() {
        return {true, decode_error::none, {}};
    }

    [[nodiscard]] static decode_result failure(decode_error err, std::string msg = {}) {
        return {false, err, std::move(msg)};
    }

    explicit operator bool() const noexcept { return ok; }
};

// ============================================================================
// Decode Options
// ============================================================================

struct decode_options {
    // Maximum allowed dimensions (0 = use default)
    int max_width = 16384;
    int max_height = 16384;

    // Packing options for multi-image containers
    bool enable_packing = false;
    int padding = 0;
    int pack_max_width = 4096;
    int pack_max_height = 4096;
    bool power_of_two = false;
};

} // namespace onyx_image

#endif // ONYX_IMAGE_TYPES_HPP_
