#include <onyx_image/codecs/png.hpp>
#include "byte_io.hpp"
#include "decode_helpers.hpp"
#include <lodepng.h>

#include <fstream>
#include <limits>

namespace onyx_image {

namespace {

// PNG signature: 89 50 4E 47 0D 0A 1A 0A
constexpr std::uint8_t PNG_SIGNATURE[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
constexpr std::size_t PNG_SIGNATURE_SIZE = sizeof(PNG_SIGNATURE);

// IHDR chunk structure:
// Offset 8-11: IHDR length (should be 13)
// Offset 12-15: IHDR type ("IHDR" = 0x49484452)
// Offset 16-19: Width (big-endian)
// Offset 20-23: Height (big-endian)
constexpr std::size_t PNG_IHDR_LENGTH_OFFSET = 8;
constexpr std::size_t PNG_IHDR_TYPE_OFFSET = 12;
constexpr std::size_t PNG_IHDR_WIDTH_OFFSET = 16;
constexpr std::size_t PNG_IHDR_HEIGHT_OFFSET = 20;
constexpr std::size_t PNG_MIN_SIZE_FOR_DIMENSIONS = 24;  // signature + IHDR length/type + width/height
constexpr std::uint32_t PNG_IHDR_TYPE = 0x49484452;  // "IHDR" in big-endian
constexpr std::uint32_t PNG_IHDR_LENGTH = 13;  // IHDR data is always 13 bytes

} // namespace

// ============================================================================
// PNG Decoder
// ============================================================================

bool png_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    if (data.size() < PNG_SIGNATURE_SIZE) {
        return false;
    }

    for (std::size_t i = 0; i < PNG_SIGNATURE_SIZE; ++i) {
        if (data[i] != PNG_SIGNATURE[i]) {
            return false;
        }
    }

    return true;
}

decode_result png_decoder::decode(std::span<const std::uint8_t> data,
                                   surface& surf,
                                   const decode_options& options) {
    if (!sniff(data)) {
        return decode_result::failure(decode_error::invalid_format, "Not a valid PNG file");
    }

    // Pre-decode dimension check from IHDR chunk to prevent loading huge images
    if (data.size() >= PNG_MIN_SIZE_FOR_DIMENSIONS) {
        // Validate IHDR chunk before reading dimensions
        std::uint32_t ihdr_length = read_be32(data.data() + PNG_IHDR_LENGTH_OFFSET);
        std::uint32_t ihdr_type = read_be32(data.data() + PNG_IHDR_TYPE_OFFSET);

        // Only perform pre-check if this looks like a valid IHDR chunk
        if (ihdr_length == PNG_IHDR_LENGTH && ihdr_type == PNG_IHDR_TYPE) {
            std::uint32_t ihdr_width = read_be32(data.data() + PNG_IHDR_WIDTH_OFFSET);
            std::uint32_t ihdr_height = read_be32(data.data() + PNG_IHDR_HEIGHT_OFFSET);

            // Check for values that would overflow when cast to int
            constexpr auto max_int = static_cast<std::uint32_t>(std::numeric_limits<int>::max());
            if (ihdr_width > max_int || ihdr_height > max_int) {
                return decode_result::failure(decode_error::dimensions_exceeded,
                    "PNG dimensions exceed maximum supported size");
            }

            auto result = validate_dimensions(static_cast<int>(ihdr_width),
                                               static_cast<int>(ihdr_height), options);
            if (!result) return result;
        }
        // If IHDR validation fails, skip pre-check and let lodepng handle it
    }

    unsigned width = 0;
    unsigned height = 0;
    std::vector<std::uint8_t> pixels;

    // Decode as RGBA
    unsigned error = lodepng::decode(pixels, width, height, data.data(), data.size());
    if (error) {
        return decode_result::failure(decode_error::invalid_format,
            std::string("PNG decode error: ") + lodepng_error_text(error));
    }

    // Check for values that would overflow when cast to int
    constexpr auto max_int = static_cast<unsigned>(std::numeric_limits<int>::max());
    if (width > max_int || height > max_int) {
        return decode_result::failure(decode_error::dimensions_exceeded,
            "PNG dimensions exceed maximum supported size");
    }

    // Post-decode dimension check (fallback if IHDR pre-check was skipped)
    auto result = validate_dimensions(static_cast<int>(width), static_cast<int>(height), options);
    if (!result) return result;

    // Allocate surface as RGBA
    if (!surf.set_size(static_cast<int>(width), static_cast<int>(height), pixel_format::rgba8888)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    // Copy pixels to surface
    write_rows(surf, pixels.data(), width * 4, static_cast<int>(height));

    return decode_result::success();
}

// ============================================================================
// PNG Encoder
// ============================================================================

std::vector<std::uint8_t> encode_png(const memory_surface& surf) {
    if (surf.width() <= 0 || surf.height() <= 0) {
        return {};
    }

    std::vector<std::uint8_t> rgba_pixels;
    const auto w = static_cast<unsigned>(surf.width());
    const auto h = static_cast<unsigned>(surf.height());

    // Convert to RGBA format for lodepng
    switch (surf.format()) {
        case pixel_format::rgba8888: {
            // Direct copy
            rgba_pixels.assign(surf.pixels().begin(), surf.pixels().end());
            break;
        }
        case pixel_format::rgb888: {
            // Add alpha channel
            rgba_pixels.resize(w * h * 4);
            const auto* src = surf.pixels().data();
            auto* dst = rgba_pixels.data();
            for (unsigned i = 0; i < w * h; ++i) {
                dst[i * 4 + 0] = src[i * 3 + 0];
                dst[i * 4 + 1] = src[i * 3 + 1];
                dst[i * 4 + 2] = src[i * 3 + 2];
                dst[i * 4 + 3] = 255;
            }
            break;
        }
        case pixel_format::indexed8: {
            // Convert indexed to RGBA using palette
            rgba_pixels.resize(w * h * 4);
            const auto* indices = surf.pixels().data();
            const auto palette = surf.palette();
            auto* dst = rgba_pixels.data();

            for (unsigned i = 0; i < w * h; ++i) {
                const std::size_t idx = indices[i];
                const std::size_t pal_offset = idx * 3;

                if (pal_offset + 2 < palette.size()) {
                    dst[i * 4 + 0] = palette[pal_offset + 0];
                    dst[i * 4 + 1] = palette[pal_offset + 1];
                    dst[i * 4 + 2] = palette[pal_offset + 2];
                } else {
                    // Missing palette entry - use black
                    dst[i * 4 + 0] = 0;
                    dst[i * 4 + 1] = 0;
                    dst[i * 4 + 2] = 0;
                }
                dst[i * 4 + 3] = 255;
            }
            break;
        }
    }

    std::vector<std::uint8_t> png_data;
    unsigned error = lodepng::encode(png_data, rgba_pixels, w, h);
    if (error) {
        return {};
    }

    return png_data;
}

bool save_png(const memory_surface& surf, const std::filesystem::path& path) {
    auto png_data = encode_png(surf);
    if (png_data.empty()) {
        return false;
    }

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    file.write(reinterpret_cast<const char*>(png_data.data()),
               static_cast<std::streamsize>(png_data.size()));

    return file.good();
}

// ============================================================================
// PNG Surface
// ============================================================================

std::vector<std::uint8_t> png_surface::encode() const {
    return encode_png(*this);
}

bool png_surface::save(const std::filesystem::path& path) const {
    return save_png(*this, path);
}

} // namespace onyx_image
