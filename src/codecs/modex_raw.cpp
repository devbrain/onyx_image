#include <onyx_image/codecs/modex_raw.hpp>
#include <onyx_image/palettes.hpp>
#include "decode_helpers.hpp"

#include <vector>

namespace onyx_image {

namespace {

// Set up VGA 256-color palette on surface
void setup_vga_palette(surface& surf) {
    auto palette = vga_default_palette();
    surf.set_palette_size(256);
    surf.write_palette(0, std::span<const std::uint8_t>(palette.data(), palette.size()));
}

} // namespace

decode_result decode_modex_graphic_planar(std::span<const std::uint8_t> data,
                                           surface& surf,
                                           int width, int height) {
    if (width <= 0 || height <= 0) {
        return decode_result::failure(decode_error::invalid_format, "Invalid dimensions");
    }

    // Bytes per row per plane (width / 4, rounded up)
    std::size_t bytes_per_plane_row = (static_cast<std::size_t>(width) + 3) / 4;
    std::size_t plane_size = bytes_per_plane_row * static_cast<std::size_t>(height);
    std::size_t expected_size = plane_size * 4;

    if (data.size() < expected_size) {
        return decode_result::failure(decode_error::truncated_data,
            "Mode X graphic-planar data too small");
    }

    if (!surf.set_size(width, height, pixel_format::indexed8)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    setup_vga_palette(surf);

    std::vector<std::uint8_t> row_pixels(static_cast<std::size_t>(width));

    for (int y = 0; y < height; ++y) {
        // Reconstruct pixels from the 4 planes
        for (int x = 0; x < width; ++x) {
            int plane = modex_plane_for_x(x);
            int offset = modex_offset_for_x(x);

            std::size_t plane_offset = static_cast<std::size_t>(plane) * plane_size;
            std::size_t byte_offset = plane_offset +
                                      static_cast<std::size_t>(y) * bytes_per_plane_row +
                                      static_cast<std::size_t>(offset);

            row_pixels[static_cast<std::size_t>(x)] = data[byte_offset];
        }

        surf.write_pixels(0, y, width, row_pixels.data());
    }

    return decode_result::success();
}

decode_result decode_modex_row_planar(std::span<const std::uint8_t> data,
                                       surface& surf,
                                       int width, int height) {
    if (width <= 0 || height <= 0) {
        return decode_result::failure(decode_error::invalid_format, "Invalid dimensions");
    }

    std::size_t bytes_per_plane_row = (static_cast<std::size_t>(width) + 3) / 4;
    std::size_t row_size = bytes_per_plane_row * 4;  // All 4 planes for one row
    std::size_t expected_size = row_size * static_cast<std::size_t>(height);

    if (data.size() < expected_size) {
        return decode_result::failure(decode_error::truncated_data,
            "Mode X row-planar data too small");
    }

    if (!surf.set_size(width, height, pixel_format::indexed8)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    setup_vga_palette(surf);

    std::vector<std::uint8_t> row_pixels(static_cast<std::size_t>(width));

    for (int y = 0; y < height; ++y) {
        std::size_t row_offset = static_cast<std::size_t>(y) * row_size;

        for (int x = 0; x < width; ++x) {
            int plane = modex_plane_for_x(x);
            int offset = modex_offset_for_x(x);

            std::size_t byte_offset = row_offset +
                                      static_cast<std::size_t>(plane) * bytes_per_plane_row +
                                      static_cast<std::size_t>(offset);

            row_pixels[static_cast<std::size_t>(x)] = data[byte_offset];
        }

        surf.write_pixels(0, y, width, row_pixels.data());
    }

    return decode_result::success();
}

decode_result decode_modex_byte_planar(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        int width, int height) {
    if (width <= 0 || height <= 0) {
        return decode_result::failure(decode_error::invalid_format, "Invalid dimensions");
    }

    // Number of 4-pixel groups per row
    std::size_t groups_per_row = (static_cast<std::size_t>(width) + 3) / 4;
    std::size_t expected_size = groups_per_row * 4 * static_cast<std::size_t>(height);

    if (data.size() < expected_size) {
        return decode_result::failure(decode_error::truncated_data,
            "Mode X byte-planar data too small");
    }

    if (!surf.set_size(width, height, pixel_format::indexed8)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    setup_vga_palette(surf);

    std::vector<std::uint8_t> row_pixels(static_cast<std::size_t>(width));
    std::size_t src_pos = 0;

    for (int y = 0; y < height; ++y) {
        int x = 0;

        // Process 4 pixels at a time
        for (std::size_t group = 0; group < groups_per_row; ++group) {
            // Read 4 bytes (one for each plane)
            std::uint8_t p0 = data[src_pos++];
            std::uint8_t p1 = data[src_pos++];
            std::uint8_t p2 = data[src_pos++];
            std::uint8_t p3 = data[src_pos++];

            // Interleave: pixel 0 from plane 0, pixel 1 from plane 1, etc.
            if (x < width) row_pixels[static_cast<std::size_t>(x++)] = p0;
            if (x < width) row_pixels[static_cast<std::size_t>(x++)] = p1;
            if (x < width) row_pixels[static_cast<std::size_t>(x++)] = p2;
            if (x < width) row_pixels[static_cast<std::size_t>(x++)] = p3;
        }

        surf.write_pixels(0, y, width, row_pixels.data());
    }

    return decode_result::success();
}

decode_result decode_modex_linear(std::span<const std::uint8_t> data,
                                   surface& surf,
                                   int width, int height) {
    if (width <= 0 || height <= 0) {
        return decode_result::failure(decode_error::invalid_format, "Invalid dimensions");
    }

    std::size_t expected_size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    if (data.size() < expected_size) {
        return decode_result::failure(decode_error::truncated_data,
            "Mode X linear data too small");
    }

    if (!surf.set_size(width, height, pixel_format::indexed8)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    setup_vga_palette(surf);

    // Copy rows directly
    write_rows(surf, data.data(), static_cast<std::size_t>(width), height);

    return decode_result::success();
}

decode_result decode_modex_raw(std::span<const std::uint8_t> data,
                                surface& surf,
                                const modex_raw_options& opts) {
    switch (opts.format) {
        case modex_format::graphic_planar:
            return decode_modex_graphic_planar(data, surf, opts.width, opts.height);

        case modex_format::row_planar:
            return decode_modex_row_planar(data, surf, opts.width, opts.height);

        case modex_format::byte_planar:
            return decode_modex_byte_planar(data, surf, opts.width, opts.height);

        case modex_format::linear:
            return decode_modex_linear(data, surf, opts.width, opts.height);
    }

    return decode_result::failure(decode_error::invalid_format, "Unknown Mode X format");
}

} // namespace onyx_image
