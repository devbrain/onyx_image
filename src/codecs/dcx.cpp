#include <onyx_image/codecs/dcx.hpp>
#include <onyx_image/codecs/pcx.hpp>
#include "byte_io.hpp"

#include <cstring>
#include <vector>

namespace onyx_image {

namespace {

// DCX magic number (little-endian)
constexpr std::uint32_t DCX_MAGIC = 0x3ADE68B1;

// Maximum number of pages in DCX
constexpr std::size_t DCX_MAX_PAGES = 1023;

// Collect all valid page offsets from DCX header
std::vector<std::uint32_t> get_page_offsets(std::span<const std::uint8_t> data) {
    std::vector<std::uint32_t> offsets;
    std::size_t file_size = data.size();

    for (std::size_t i = 0; i < DCX_MAX_PAGES && (4 + (i + 1) * 4) <= data.size(); ++i) {
        std::uint32_t offset = read_le32(data.data() + 4 + i * 4);
        if (offset == 0) {
            break;  // End of page list
        }
        if (offset < file_size) {
            offsets.push_back(offset);
        }
    }
    return offsets;
}

}  // namespace

bool dcx_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    if (data.size() < 4) {
        return false;
    }
    return read_le32(data.data()) == DCX_MAGIC;
}

decode_result dcx_decoder::decode(std::span<const std::uint8_t> data,
                                   surface& surf,
                                   const decode_options& options) {
    if (!sniff(data)) {
        return decode_result::failure(decode_error::invalid_format, "Not a valid DCX file");
    }

    if (data.size() < 8) {
        return decode_result::failure(decode_error::truncated_data, "DCX file too small");
    }

    // Get all page offsets
    auto offsets = get_page_offsets(data);
    if (offsets.empty()) {
        return decode_result::failure(decode_error::invalid_format, "DCX file has no pages");
    }

    // Check dimension limits early
    const int max_w = options.max_width > 0 ? options.max_width : 16384;
    const int max_h = options.max_height > 0 ? options.max_height : 16384;

    // First pass: parse headers to get dimensions of all pages
    struct page_info {
        int width;
        int height;
        std::span<const std::uint8_t> pcx_data;
    };
    std::vector<page_info> pages;
    pages.reserve(offsets.size());

    std::size_t atlas_width = 0;
    std::size_t atlas_height = 0;
    pixel_format common_format = pixel_format::indexed8;

    for (std::size_t i = 0; i < offsets.size(); ++i) {
        std::uint32_t start = offsets[i];
        std::uint32_t end = (i + 1 < offsets.size()) ? offsets[i + 1] : static_cast<std::uint32_t>(data.size());

        if (start >= end || start >= data.size()) {
            continue;
        }

        auto pcx_data = data.subspan(start, end - start);

        // Parse PCX header to get dimensions
        pcx_decoder::header_info info;
        auto result = pcx_decoder::parse_header(pcx_data, info, options);
        if (!result) {
            continue;  // Skip invalid pages
        }

        pages.push_back({info.width, info.height, pcx_data});

        // Track atlas dimensions (stack vertically) with overflow protection
        atlas_width = std::max(atlas_width, static_cast<std::size_t>(info.width));
        atlas_height += static_cast<std::size_t>(info.height);

        // Check atlas dimensions against limits during accumulation
        if (atlas_width > static_cast<std::size_t>(max_w) ||
            atlas_height > static_cast<std::size_t>(max_h)) {
            return decode_result::failure(decode_error::dimensions_exceeded,
                "Combined DCX atlas dimensions exceed limits");
        }

        // Check format - use RGB if any page has more than 256 colors
        if (info.bits_per_pixel * info.num_planes > 8) {
            common_format = pixel_format::rgb888;
        }
    }

    if (pages.empty()) {
        return decode_result::failure(decode_error::invalid_format, "No valid pages in DCX file");
    }

    // Allocate atlas surface
    if (!surf.set_size(static_cast<int>(atlas_width), static_cast<int>(atlas_height), common_format)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate atlas surface");
    }

    // Second pass: decode each page into the atlas
    int y_offset = 0;
    for (std::size_t i = 0; i < pages.size(); ++i) {
        const auto& page = pages[i];

        // Decode page to temporary surface
        memory_surface temp_surf;
        auto result = pcx_decoder::decode(page.pcx_data, temp_surf, options);
        if (!result) {
            // Fill with zeros and continue
            y_offset += page.height;
            continue;
        }

        // Copy palette from first page if indexed
        if (i == 0 && temp_surf.format() == pixel_format::indexed8) {
            auto pal = temp_surf.palette();
            surf.set_palette_size(static_cast<int>(pal.size() / 3));
            surf.write_palette(0, pal);
        }

        // Copy pixels to atlas at y_offset
        auto src_pixels = temp_surf.pixels();
        std::size_t bytes_per_pixel = (temp_surf.format() == pixel_format::rgb888)    ? 3
                                    : (temp_surf.format() == pixel_format::rgba8888) ? 4
                                                                                      : 1;
        std::size_t src_row_bytes = static_cast<std::size_t>(page.width) * bytes_per_pixel;

        for (int y = 0; y < page.height; ++y) {
            const std::uint8_t* src_row = src_pixels.data() + static_cast<std::size_t>(y) * src_row_bytes;
            surf.write_pixels(0, y_offset + y, static_cast<int>(src_row_bytes), src_row);
        }

        // Set subrect for this page
        subrect sr;
        sr.rect = {0, y_offset, page.width, page.height};
        sr.kind = subrect_kind::frame;
        sr.user_tag = static_cast<std::uint32_t>(i);
        surf.set_subrect(static_cast<int>(i), sr);

        y_offset += page.height;
    }

    return decode_result::success();
}

}  // namespace onyx_image
