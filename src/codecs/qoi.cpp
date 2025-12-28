#include <onyx_image/codecs/qoi.hpp>
#include "byte_io.hpp"
#include "decode_helpers.hpp"

#include <array>
#include <cstring>
#include <vector>

namespace onyx_image {

namespace {

// QOI format constants
constexpr std::uint32_t QOI_MAGIC = 0x716F6966;  // "qoif" in big-endian
constexpr std::size_t QOI_HEADER_SIZE = 14;
constexpr std::size_t QOI_END_MARKER_SIZE = 8;

// QOI chunk tags
constexpr std::uint8_t QOI_OP_INDEX = 0x00;  // 00xxxxxx
constexpr std::uint8_t QOI_OP_DIFF = 0x40;   // 01xxxxxx
constexpr std::uint8_t QOI_OP_LUMA = 0x80;   // 10xxxxxx
constexpr std::uint8_t QOI_OP_RUN = 0xC0;    // 11xxxxxx
constexpr std::uint8_t QOI_OP_RGB = 0xFE;    // 11111110
constexpr std::uint8_t QOI_OP_RGBA = 0xFF;   // 11111111

constexpr std::uint8_t QOI_MASK_2 = 0xC0;  // Top 2 bits mask

// Color hash function for index lookup
inline std::size_t qoi_color_hash(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    return (static_cast<std::size_t>(r) * 3 + static_cast<std::size_t>(g) * 5 +
            static_cast<std::size_t>(b) * 7 + static_cast<std::size_t>(a) * 11) %
           64;
}

}  // namespace

bool qoi_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    if (data.size() < QOI_HEADER_SIZE) {
        return false;
    }

    // Check magic "qoif"
    std::uint32_t magic = read_be32(data.data());
    if (magic != QOI_MAGIC) {
        return false;
    }

    // Validate dimensions are non-zero
    std::uint32_t width = read_be32(data.data() + 4);
    std::uint32_t height = read_be32(data.data() + 8);
    if (width == 0 || height == 0) {
        return false;
    }

    // Validate channels (3 or 4)
    std::uint8_t channels = data[12];
    if (channels != 3 && channels != 4) {
        return false;
    }

    // Validate colorspace (0 or 1)
    std::uint8_t colorspace = data[13];
    if (colorspace > 1) {
        return false;
    }

    return true;
}

decode_result qoi_decoder::decode(std::span<const std::uint8_t> data,
                                   surface& surf,
                                   const decode_options& options) {
    if (data.size() < QOI_HEADER_SIZE + QOI_END_MARKER_SIZE) {
        return decode_result::failure(decode_error::truncated_data, "QOI file too small");
    }

    // Parse header
    std::uint32_t magic = read_be32(data.data());
    if (magic != QOI_MAGIC) {
        return decode_result::failure(decode_error::invalid_format, "Invalid QOI magic");
    }

    std::uint32_t width = read_be32(data.data() + 4);
    std::uint32_t height = read_be32(data.data() + 8);
    std::uint8_t channels = data[12];
    // std::uint8_t colorspace = data[13];  // Not used for decoding

    if (width == 0 || height == 0) {
        return decode_result::failure(decode_error::invalid_format, "Invalid QOI dimensions");
    }

    if (channels != 3 && channels != 4) {
        return decode_result::failure(decode_error::invalid_format, "Invalid QOI channel count");
    }

    // Check dimension limits
    auto result = validate_dimensions(static_cast<int>(width), static_cast<int>(height), options);
    if (!result) return result;

    // Prevent overflow
    constexpr std::uint64_t MAX_PIXELS = 400000000ULL;  // ~400 megapixels
    std::uint64_t total_pixels = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
    if (total_pixels > MAX_PIXELS) {
        return decode_result::failure(decode_error::dimensions_exceeded, "QOI image too large");
    }

    // Allocate surface - always decode to RGBA for simplicity
    pixel_format fmt = (channels == 4) ? pixel_format::rgba8888 : pixel_format::rgb888;
    if (!surf.set_size(static_cast<int>(width), static_cast<int>(height), fmt)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    // Initialize color index array
    struct rgba_t {
        std::uint8_t r, g, b, a;
    };
    std::array<rgba_t, 64> index{};

    // Current pixel
    rgba_t px{0, 0, 0, 255};

    // Output buffer
    std::size_t pixel_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<std::uint8_t> pixels(pixel_count * channels);

    std::size_t src_pos = QOI_HEADER_SIZE;
    std::size_t src_end = data.size() - QOI_END_MARKER_SIZE;
    std::size_t dst_pos = 0;
    int run = 0;

    for (std::size_t px_idx = 0; px_idx < pixel_count; ++px_idx) {
        if (run > 0) {
            run--;
        } else if (src_pos < src_end) {
            std::uint8_t b1 = data[src_pos++];

            if (b1 == QOI_OP_RGB) {
                if (src_pos + 3 > src_end) {
                    return decode_result::failure(decode_error::truncated_data, "QOI RGB chunk truncated");
                }
                px.r = data[src_pos++];
                px.g = data[src_pos++];
                px.b = data[src_pos++];
            } else if (b1 == QOI_OP_RGBA) {
                if (src_pos + 4 > src_end) {
                    return decode_result::failure(decode_error::truncated_data, "QOI RGBA chunk truncated");
                }
                px.r = data[src_pos++];
                px.g = data[src_pos++];
                px.b = data[src_pos++];
                px.a = data[src_pos++];
            } else if ((b1 & QOI_MASK_2) == QOI_OP_INDEX) {
                px = index[b1];
            } else if ((b1 & QOI_MASK_2) == QOI_OP_DIFF) {
                px.r += static_cast<std::uint8_t>(((b1 >> 4) & 0x03) - 2);
                px.g += static_cast<std::uint8_t>(((b1 >> 2) & 0x03) - 2);
                px.b += static_cast<std::uint8_t>((b1 & 0x03) - 2);
            } else if ((b1 & QOI_MASK_2) == QOI_OP_LUMA) {
                if (src_pos >= src_end) {
                    return decode_result::failure(decode_error::truncated_data, "QOI LUMA chunk truncated");
                }
                std::uint8_t b2 = data[src_pos++];
                int vg = (b1 & 0x3F) - 32;
                px.r += static_cast<std::uint8_t>(vg - 8 + ((b2 >> 4) & 0x0F));
                px.g += static_cast<std::uint8_t>(vg);
                px.b += static_cast<std::uint8_t>(vg - 8 + (b2 & 0x0F));
            } else if ((b1 & QOI_MASK_2) == QOI_OP_RUN) {
                run = (b1 & 0x3F);
            }

            index[qoi_color_hash(px.r, px.g, px.b, px.a)] = px;
        }

        // Write pixel to output
        if (channels == 4) {
            pixels[dst_pos++] = px.r;
            pixels[dst_pos++] = px.g;
            pixels[dst_pos++] = px.b;
            pixels[dst_pos++] = px.a;
        } else {
            pixels[dst_pos++] = px.r;
            pixels[dst_pos++] = px.g;
            pixels[dst_pos++] = px.b;
        }
    }

    // Write pixels to surface
    write_rows(surf, pixels.data(), static_cast<std::size_t>(width) * channels, static_cast<int>(height));

    return decode_result::success();
}

}  // namespace onyx_image
