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

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

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

// ----------------------------------------------------------------------------
// GIF logical-screen repair
//
// Some DOS-era GIF encoders write a Logical Screen Descriptor with width and/or
// height set to 0 and store the real dimensions only in the first Image
// Descriptor. stb_image enforces that every frame fits within the logical
// screen, so it rejects these files with "bad Image Descriptor". Lenient
// decoders (PIL, ImageMagick) derive the canvas from the image descriptors
// instead. We do the same: scan the frames, compute the bounding canvas, and
// patch the zero field(s) on a private copy before handing it to stb_image.
// ----------------------------------------------------------------------------

constexpr int gif_read_u16le(std::span<const std::uint8_t> data, std::size_t off) noexcept {
    return static_cast<int>(data[off]) | (static_cast<int>(data[off + 1]) << 8);
}

// Skip a chain of size-prefixed GIF sub-blocks, returning the position just
// past the terminating zero-length block.
std::size_t gif_skip_sub_blocks(std::span<const std::uint8_t> data, std::size_t pos) noexcept {
    while (pos < data.size()) {
        const std::uint8_t n = data[pos++];
        if (n == 0) break;
        pos += n;
    }
    return pos;
}

// Walk every Image Descriptor and return the canvas size required to contain
// all frames (max x+w, max y+h). Returns {0, 0} if none are found.
struct gif_extent { int w; int h; };

gif_extent gif_required_canvas(std::span<const std::uint8_t> data) noexcept {
    constexpr std::size_t header_size = 13; // signature(6) + LSD(7)
    if (data.size() < header_size) return {0, 0};

    std::size_t pos = header_size;
    const std::uint8_t packed = data[10];
    if (packed & 0x80) { // global color table present
        pos += (std::size_t{2} << (packed & 0x07)) * 3;
    }

    int max_w = 0;
    int max_h = 0;
    while (pos < data.size()) {
        const std::uint8_t block = data[pos++];
        if (block == 0x3B) { // trailer
            break;
        } else if (block == 0x21) { // extension
            if (pos >= data.size()) break;
            ++pos; // label
            pos = gif_skip_sub_blocks(data, pos);
        } else if (block == 0x2C) { // image descriptor
            if (pos + 9 > data.size()) break;
            const int x = gif_read_u16le(data, pos + 0);
            const int y = gif_read_u16le(data, pos + 2);
            const int w = gif_read_u16le(data, pos + 4);
            const int h = gif_read_u16le(data, pos + 6);
            const std::uint8_t img_packed = data[pos + 8];
            pos += 9;
            max_w = std::max(max_w, x + w);
            max_h = std::max(max_h, y + h);
            if (img_packed & 0x80) { // local color table present
                pos += (std::size_t{2} << (img_packed & 0x07)) * 3;
            }
            if (pos >= data.size()) break;
            ++pos; // LZW minimum code size
            pos = gif_skip_sub_blocks(data, pos);
        } else {
            break; // unexpected byte; stop scanning
        }
    }
    return {max_w, max_h};
}

// If the logical screen width/height is zero, return a patched copy of the GIF
// with the field(s) filled in from the image descriptors. Returns nullopt when
// no patch is needed or a sensible canvas could not be derived (in which case
// the original data is handed to stb_image unchanged).
std::optional<std::vector<std::uint8_t>> gif_patch_zero_screen(std::span<const std::uint8_t> data) {
    if (data.size() < 13) return std::nullopt;

    const bool zero_w = gif_read_u16le(data, 6) == 0;
    const bool zero_h = gif_read_u16le(data, 8) == 0;
    if (!zero_w && !zero_h) return std::nullopt;

    const gif_extent canvas = gif_required_canvas(data);
    const bool need_w = zero_w && (canvas.w <= 0 || canvas.w > 0xFFFF);
    const bool need_h = zero_h && (canvas.h <= 0 || canvas.h > 0xFFFF);
    if (need_w || need_h) return std::nullopt; // can't fix; let stb report it

    std::vector<std::uint8_t> copy(data.begin(), data.end());
    if (zero_w) {
        copy[6] = static_cast<std::uint8_t>(canvas.w & 0xFF);
        copy[7] = static_cast<std::uint8_t>((canvas.w >> 8) & 0xFF);
    }
    if (zero_h) {
        copy[8] = static_cast<std::uint8_t>(canvas.h & 0xFF);
        copy[9] = static_cast<std::uint8_t>((canvas.h >> 8) & 0xFF);
    }
    return copy;
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

    // Repair GIFs that declare a 0-sized logical screen (see gif_patch_zero_screen).
    if (auto patched = gif_patch_zero_screen(data)) {
        return stb_decode_common(*patched, surf, options);
    }

    return stb_decode_common(data, surf, options);
}

} // namespace onyx_image
