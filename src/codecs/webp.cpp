#include <onyx_image/codecs/webp.hpp>
#include "decode_helpers.hpp"

#include <tiny_webp.h>

#include <cstdlib>
#include <limits>
#include <memory>

#define twp_NO_SIMD
#define twp_IMPLEMENTATION
#include <tiny_webp.h>

namespace onyx_image {

namespace {

constexpr std::size_t WEBP_MIN_HEADER_SIZE = 12;

bool has_webp_signature(std::span<const std::uint8_t> data) noexcept {
    return data.size() >= WEBP_MIN_HEADER_SIZE &&
           data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' &&
           data[8] == 'W' && data[9] == 'E' && data[10] == 'B' && data[11] == 'P';
}

} // namespace

bool webp_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    return has_webp_signature(data);
}

decode_result webp_decoder::decode(std::span<const std::uint8_t> data,
                                    surface& surf,
                                    const decode_options& options) {
    if (!sniff(data)) {
        return decode_result::failure(decode_error::invalid_format, "Not a valid WebP file");
    }

    if (data.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return decode_result::failure(decode_error::truncated_data,
            "Input data exceeds maximum supported size");
    }

    const auto data_len = static_cast<int>(data.size());
    auto* mutable_data = const_cast<std::uint8_t*>(data.data());

    int info_width = 0;
    int info_height = 0;
    if (!twp_get_info_from_memory(mutable_data, data_len, &info_width, &info_height, nullptr, nullptr)) {
        return decode_result::failure(decode_error::invalid_format, "Invalid or unsupported WebP file");
    }

    if (info_width <= 0 || info_height <= 0) {
        return decode_result::failure(decode_error::invalid_format, "Invalid WebP dimensions");
    }

    auto result = validate_dimensions(info_width, info_height, options);
    if (!result) return result;

    int width = 0;
    int height = 0;
    unsigned char* pixels = twp_read_from_memory(mutable_data, data_len, &width, &height,
                                                 twp_FORMAT_RGBA, 0);
    if (!pixels) {
        return decode_result::failure(decode_error::invalid_format, "WebP decode failed");
    }

    std::unique_ptr<unsigned char, decltype(&std::free)> pixel_guard(pixels, std::free);

    if (width <= 0 || height <= 0) {
        return decode_result::failure(decode_error::invalid_format, "Invalid WebP dimensions");
    }

    result = validate_dimensions(width, height, options);
    if (!result) return result;

    if (!surf.set_size(width, height, pixel_format::rgba8888)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    write_rows(surf, pixels, static_cast<std::size_t>(width) * 4, height);

    return decode_result::success();
}

} // namespace onyx_image
