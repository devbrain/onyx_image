#include <onyx_image/codecs/ega_raw.hpp>
#include <onyx_image/palettes.hpp>
#include "decode_helpers.hpp"

#include <vector>
#include <cstring>

namespace onyx_image {

namespace {

// Set up EGA palette on surface based on number of planes
void setup_ega_palette(surface& surf, int num_planes) {
    auto full_palette = ega_default_palette();
    int num_colors = 1 << num_planes;  // 2, 4, 8, or 16 colors

    surf.set_palette_size(num_colors);
    surf.write_palette(0, std::span<const std::uint8_t>(
        full_palette.data(), static_cast<std::size_t>(num_colors) * 3));
}

// Extract bit from byte (MSB first, bit 7 = pixel 0)
inline int get_bit(std::uint8_t byte, int bit_index) {
    return (byte >> (7 - bit_index)) & 1;
}

} // namespace

std::size_t ega_raw_data_size(int width, int height,
                               ega_format format,
                               int num_planes) noexcept {
    if (width <= 0 || height <= 0 || num_planes <= 0) {
        return 0;
    }

    std::size_t w = static_cast<std::size_t>(width);
    std::size_t h = static_cast<std::size_t>(height);
    std::size_t planes = static_cast<std::size_t>(num_planes);

    switch (format) {
        case ega_format::graphic_planar:
        case ega_format::row_planar:
        case ega_format::byte_planar:
            // Planar formats: each plane has 1 bit per pixel
            // Bytes per row = (width + 7) / 8, rounded up
            return ((w + 7) / 8) * h * planes;

        case ega_format::linear:
            // Linear: 4 bits per pixel, 2 pixels per byte
            return ((w + 1) / 2) * h;
    }

    return 0;
}

decode_result decode_ega_graphic_planar(std::span<const std::uint8_t> data,
                                         surface& surf,
                                         int width, int height,
                                         int num_planes,
                                         ega_plane_order plane_order) {
    if (width <= 0 || height <= 0) {
        return decode_result::failure(decode_error::invalid_format, "Invalid dimensions");
    }

    if (num_planes < 1 || num_planes > 4) {
        return decode_result::failure(decode_error::invalid_format, "Invalid plane count");
    }

    std::size_t bytes_per_row = (static_cast<std::size_t>(width) + 7) / 8;
    std::size_t plane_size = bytes_per_row * static_cast<std::size_t>(height);
    std::size_t expected_size = plane_size * static_cast<std::size_t>(num_planes);

    if (data.size() < expected_size) {
        return decode_result::failure(decode_error::truncated_data,
            "EGA graphic-planar data too small");
    }

    if (!surf.set_size(width, height, pixel_format::indexed8)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    setup_ega_palette(surf, num_planes);

    std::vector<std::uint8_t> row_pixels(static_cast<std::size_t>(width));

    for (int y = 0; y < height; ++y) {
        std::memset(row_pixels.data(), 0, row_pixels.size());

        for (int plane = 0; plane < num_planes; ++plane) {
            int bit_pos = ega_plane_bit(plane_order, plane);
            std::size_t plane_offset = static_cast<std::size_t>(plane) * plane_size;
            std::size_t row_offset = plane_offset + static_cast<std::size_t>(y) * bytes_per_row;

            for (int x = 0; x < width; ++x) {
                std::size_t byte_idx = row_offset + static_cast<std::size_t>(x) / 8;
                int bit_idx = x % 8;

                if (get_bit(data[byte_idx], bit_idx)) {
                    row_pixels[static_cast<std::size_t>(x)] |= static_cast<std::uint8_t>(1 << bit_pos);
                }
            }
        }

        surf.write_pixels(0, y, width, row_pixels.data());
    }

    return decode_result::success();
}

decode_result decode_ega_row_planar(std::span<const std::uint8_t> data,
                                     surface& surf,
                                     int width, int height,
                                     int num_planes,
                                     ega_plane_order plane_order) {
    if (width <= 0 || height <= 0) {
        return decode_result::failure(decode_error::invalid_format, "Invalid dimensions");
    }

    if (num_planes < 1 || num_planes > 4) {
        return decode_result::failure(decode_error::invalid_format, "Invalid plane count");
    }

    std::size_t bytes_per_row = (static_cast<std::size_t>(width) + 7) / 8;
    std::size_t row_size = bytes_per_row * static_cast<std::size_t>(num_planes);
    std::size_t expected_size = row_size * static_cast<std::size_t>(height);

    if (data.size() < expected_size) {
        return decode_result::failure(decode_error::truncated_data,
            "EGA row-planar data too small");
    }

    if (!surf.set_size(width, height, pixel_format::indexed8)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    setup_ega_palette(surf, num_planes);

    std::vector<std::uint8_t> row_pixels(static_cast<std::size_t>(width));

    for (int y = 0; y < height; ++y) {
        std::memset(row_pixels.data(), 0, row_pixels.size());
        std::size_t row_offset = static_cast<std::size_t>(y) * row_size;

        for (int plane = 0; plane < num_planes; ++plane) {
            int bit_pos = ega_plane_bit(plane_order, plane);
            std::size_t plane_offset = row_offset + static_cast<std::size_t>(plane) * bytes_per_row;

            for (int x = 0; x < width; ++x) {
                std::size_t byte_idx = plane_offset + static_cast<std::size_t>(x) / 8;
                int bit_idx = x % 8;

                if (get_bit(data[byte_idx], bit_idx)) {
                    row_pixels[static_cast<std::size_t>(x)] |= static_cast<std::uint8_t>(1 << bit_pos);
                }
            }
        }

        surf.write_pixels(0, y, width, row_pixels.data());
    }

    return decode_result::success();
}

decode_result decode_ega_byte_planar(std::span<const std::uint8_t> data,
                                      surface& surf,
                                      int width, int height,
                                      int num_planes,
                                      ega_plane_order plane_order) {
    if (width <= 0 || height <= 0) {
        return decode_result::failure(decode_error::invalid_format, "Invalid dimensions");
    }

    if (num_planes < 1 || num_planes > 4) {
        return decode_result::failure(decode_error::invalid_format, "Invalid plane count");
    }

    std::size_t bytes_per_row = (static_cast<std::size_t>(width) + 7) / 8;
    std::size_t expected_size = bytes_per_row * static_cast<std::size_t>(height) *
                                 static_cast<std::size_t>(num_planes);

    if (data.size() < expected_size) {
        return decode_result::failure(decode_error::truncated_data,
            "EGA byte-planar data too small");
    }

    if (!surf.set_size(width, height, pixel_format::indexed8)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    setup_ega_palette(surf, num_planes);

    std::vector<std::uint8_t> row_pixels(static_cast<std::size_t>(width));
    std::size_t src_pos = 0;

    for (int y = 0; y < height; ++y) {
        std::memset(row_pixels.data(), 0, row_pixels.size());

        // Process 8 pixels at a time
        for (std::size_t byte_x = 0; byte_x < bytes_per_row; ++byte_x) {
            // Read num_planes bytes for this 8-pixel block
            for (int plane = 0; plane < num_planes; ++plane) {
                if (src_pos >= data.size()) {
                    return decode_result::failure(decode_error::truncated_data,
                        "EGA byte-planar data truncated");
                }

                std::uint8_t plane_byte = data[src_pos++];
                int bit_pos = ega_plane_bit(plane_order, plane);

                // Extract 8 bits from this plane byte
                for (int bit = 0; bit < 8; ++bit) {
                    int x = static_cast<int>(byte_x * 8) + bit;
                    if (x < width) {
                        if (get_bit(plane_byte, bit)) {
                            row_pixels[static_cast<std::size_t>(x)] |=
                                static_cast<std::uint8_t>(1 << bit_pos);
                        }
                    }
                }
            }
        }

        surf.write_pixels(0, y, width, row_pixels.data());
    }

    return decode_result::success();
}

decode_result decode_ega_linear(std::span<const std::uint8_t> data,
                                 surface& surf,
                                 int width, int height,
                                 bool high_nibble_first) {
    if (width <= 0 || height <= 0) {
        return decode_result::failure(decode_error::invalid_format, "Invalid dimensions");
    }

    std::size_t bytes_per_row = (static_cast<std::size_t>(width) + 1) / 2;
    std::size_t expected_size = bytes_per_row * static_cast<std::size_t>(height);

    if (data.size() < expected_size) {
        return decode_result::failure(decode_error::truncated_data,
            "EGA linear data too small");
    }

    if (!surf.set_size(width, height, pixel_format::indexed8)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    setup_ega_palette(surf, 4);  // Linear format always uses 16 colors

    std::vector<std::uint8_t> row_pixels(static_cast<std::size_t>(width));
    std::size_t src_pos = 0;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 2) {
            std::uint8_t byte = data[src_pos++];

            std::uint8_t pixel0, pixel1;
            if (high_nibble_first) {
                pixel0 = (byte >> 4) & 0x0F;
                pixel1 = byte & 0x0F;
            } else {
                pixel0 = byte & 0x0F;
                pixel1 = (byte >> 4) & 0x0F;
            }

            row_pixels[static_cast<std::size_t>(x)] = pixel0;
            if (x + 1 < width) {
                row_pixels[static_cast<std::size_t>(x + 1)] = pixel1;
            }
        }

        surf.write_pixels(0, y, width, row_pixels.data());
    }

    return decode_result::success();
}

decode_result decode_ega_raw(std::span<const std::uint8_t> data,
                              surface& surf,
                              const ega_raw_options& opts) {
    switch (opts.format) {
        case ega_format::graphic_planar:
            return decode_ega_graphic_planar(data, surf, opts.width, opts.height,
                                              opts.num_planes, opts.plane_order);

        case ega_format::row_planar:
            return decode_ega_row_planar(data, surf, opts.width, opts.height,
                                          opts.num_planes, opts.plane_order);

        case ega_format::byte_planar:
            return decode_ega_byte_planar(data, surf, opts.width, opts.height,
                                           opts.num_planes, opts.plane_order);

        case ega_format::linear:
            return decode_ega_linear(data, surf, opts.width, opts.height,
                                      opts.high_nibble_first);
    }

    return decode_result::failure(decode_error::invalid_format, "Unknown EGA format");
}

} // namespace onyx_image
