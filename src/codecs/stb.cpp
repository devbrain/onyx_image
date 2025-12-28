// stb_image-based decoders for JPEG, TGA, GIF

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_PNG  // We use lodepng for PNG
#define STBI_NO_BMP  // Custom BMP support planned
#define STBI_NO_PSD
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM

#include <stb_image.h>

#include <onyx_image/codecs/jpeg.hpp>
#include <onyx_image/codecs/tga.hpp>
#include <onyx_image/codecs/gif.hpp>
#include "decode_helpers.hpp"

#include <cstring>
#include <limits>
#include <memory>

namespace onyx_image {

namespace {

// Common stb_image decode helper
decode_result stb_decode_common(std::span<const std::uint8_t> data,
                                 surface& surf,
                                 const decode_options& options) {
    // Guard against data size exceeding INT_MAX (stb uses int for length)
    if (data.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return decode_result::failure(decode_error::truncated_data,
            "Input data exceeds maximum supported size");
    }

    // Pre-decode dimension check to avoid loading huge images
    int info_width = 0;
    int info_height = 0;
    int info_channels = 0;
    if (stbi_info_from_memory(data.data(), static_cast<int>(data.size()),
                              &info_width, &info_height, &info_channels)) {
        auto result = validate_dimensions(info_width, info_height, options);
        if (!result) return result;
    }

    int width = 0;
    int height = 0;
    int channels = 0;

    // Request RGBA output
    constexpr int desired_channels = 4;

    stbi_uc* pixels = stbi_load_from_memory(
        data.data(),
        static_cast<int>(data.size()),
        &width,
        &height,
        &channels,
        desired_channels
    );

    if (!pixels) {
        return decode_result::failure(decode_error::invalid_format, stbi_failure_reason());
    }

    // Use unique_ptr for automatic cleanup
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixel_guard(pixels, stbi_image_free);

    // Post-decode dimension check (fallback if stbi_info_from_memory failed)
    auto result = validate_dimensions(width, height, options);
    if (!result) return result;

    if (!surf.set_size(width, height, pixel_format::rgba8888)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    // Copy pixel data to surface
    write_rows(surf, pixels, static_cast<std::size_t>(width) * 4, height);

    return decode_result::success();
}

} // namespace

// ============================================================================
// JPEG Decoder
// ============================================================================

bool jpeg_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    // JPEG starts with FFD8FF
    if (data.size() < 3) {
        return false;
    }
    return data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF;
}

decode_result jpeg_decoder::decode(std::span<const std::uint8_t> data,
                                    surface& surf,
                                    const decode_options& options) {
    if (!sniff(data)) {
        return decode_result::failure(decode_error::invalid_format, "Not a valid JPEG file");
    }
    return stb_decode_common(data, surf, options);
}

// ============================================================================
// TGA Decoder
// ============================================================================

bool tga_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    // TGA has no magic number, but we can check for valid header fields
    // Minimum TGA header is 18 bytes
    if (data.size() < 18) {
        return false;
    }

    // Check image type (byte 2): valid values are 0-3, 9-11
    const std::uint8_t image_type = data[2];
    if (image_type > 11 || (image_type > 3 && image_type < 9)) {
        return false;
    }

    // Check color map type (byte 1): must be 0 or 1
    const std::uint8_t colormap_type = data[1];
    if (colormap_type > 1) {
        return false;
    }

    // Check bits per pixel (byte 16): valid values are 8, 15, 16, 24, 32
    const std::uint8_t bpp = data[16];
    if (bpp != 8 && bpp != 15 && bpp != 16 && bpp != 24 && bpp != 32) {
        return false;
    }

    // Check if dimensions are reasonable
    const int width = data[12] | (data[13] << 8);
    const int height = data[14] | (data[15] << 8);
    if (width == 0 || height == 0 || width > 32768 || height > 32768) {
        return false;
    }

    return true;
}

decode_result tga_decoder::decode(std::span<const std::uint8_t> data,
                                   surface& surf,
                                   const decode_options& options) {
    if (!sniff(data)) {
        return decode_result::failure(decode_error::invalid_format, "Not a valid TGA file");
    }
    return stb_decode_common(data, surf, options);
}

// ============================================================================
// GIF Decoder
// ============================================================================

bool gif_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    // GIF starts with "GIF87a" or "GIF89a"
    if (data.size() < 6) {
        return false;
    }
    return data[0] == 'G' && data[1] == 'I' && data[2] == 'F' &&
           data[3] == '8' && (data[4] == '7' || data[4] == '9') && data[5] == 'a';
}

decode_result gif_decoder::decode(std::span<const std::uint8_t> data,
                                   surface& surf,
                                   const decode_options& options) {
    if (!sniff(data)) {
        return decode_result::failure(decode_error::invalid_format, "Not a valid GIF file");
    }
    return stb_decode_common(data, surf, options);
}

} // namespace onyx_image
