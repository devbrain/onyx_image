#include <onyx_image/codecs/ico.hpp>
#include <onyx_image/codecs/png.hpp>
#include "byte_io.hpp"
#include "decode_helpers.hpp"

#include <libexe/libexe.hpp>

#include <algorithm>
#include <cstring>
#include <type_traits>
#include <vector>

namespace onyx_image {

namespace {

// ICO/CUR file header
struct ico_header {
    std::uint16_t reserved;   // Must be 0
    std::uint16_t type;       // 1 = ICO, 2 = CUR
    std::uint16_t count;      // Number of images
};

// ICO directory entry (differs from EXE resource format)
struct ico_dir_entry {
    std::uint8_t width;       // Width (0 = 256)
    std::uint8_t height;      // Height (0 = 256)
    std::uint8_t color_count; // Colors (0 if >= 8bpp)
    std::uint8_t reserved;    // Reserved
    std::uint16_t planes;     // Color planes (ICO) or hotspot X (CUR)
    std::uint16_t bit_count;  // Bits per pixel (ICO) or hotspot Y (CUR)
    std::uint32_t size;       // Image data size
    std::uint32_t offset;     // Image data offset in file
};

// DIB header (BITMAPINFOHEADER)
struct dib_header {
    std::uint32_t size;
    std::int32_t width;
    std::int32_t height;      // Doubled for icons (includes AND mask)
    std::uint16_t planes;
    std::uint16_t bit_count;
    std::uint32_t compression;
    std::uint32_t size_image;
    std::int32_t x_pels_per_meter;
    std::int32_t y_pels_per_meter;
    std::uint32_t clr_used;
    std::uint32_t clr_important;
};

// Compression types
constexpr std::uint32_t BI_RGB = 0;
constexpr std::uint32_t BI_BITFIELDS = 3;


bool parse_ico_header(std::span<const std::uint8_t> data, ico_header& header) {
    if (data.size() < 6) return false;
    header.reserved = read_le16(data.data());
    header.type = read_le16(data.data() + 2);
    header.count = read_le16(data.data() + 4);
    return header.reserved == 0 && (header.type == 1 || header.type == 2);
}

bool parse_ico_dir_entry(const std::uint8_t* p, ico_dir_entry& entry) {
    entry.width = p[0];
    entry.height = p[1];
    entry.color_count = p[2];
    entry.reserved = p[3];
    entry.planes = read_le16(p + 4);
    entry.bit_count = read_le16(p + 6);
    entry.size = read_le32(p + 8);
    entry.offset = read_le32(p + 12);
    return true;
}

bool parse_dib_header(std::span<const std::uint8_t> data, dib_header& header) {
    if (data.size() < 40) return false;
    const std::uint8_t* p = data.data();
    header.size = read_le32(p);
    if (header.size < 40) return false;
    header.width = read_le32_signed(p + 4);
    header.height = read_le32_signed(p + 8);
    header.planes = read_le16(p + 12);
    header.bit_count = read_le16(p + 14);
    header.compression = read_le32(p + 16);
    header.size_image = read_le32(p + 20);
    header.x_pels_per_meter = read_le32_signed(p + 24);
    header.y_pels_per_meter = read_le32_signed(p + 28);
    header.clr_used = read_le32(p + 32);
    header.clr_important = read_le32(p + 36);
    return true;
}

// Get AND mask bit (1 = transparent, 0 = opaque)
bool get_and_mask_bit(const std::uint8_t* and_mask, int width, int x, int y) {
    std::size_t and_stride = row_stride_4byte(width, 1);
    std::size_t byte_offset = static_cast<std::size_t>(y) * and_stride + static_cast<std::size_t>(x / 8);
    int bit_index = 7 - (x % 8);
    return (and_mask[byte_offset] >> bit_index) & 1;
}

// Decoded icon image
struct decoded_icon {
    int width;
    int height;
    std::vector<std::uint8_t> pixels;  // RGBA format
};

// Decode a single icon image (DIB or PNG)
bool decode_icon_image(std::span<const std::uint8_t> data, decoded_icon& icon,
                       int max_w, int max_h) {
    if (data.size() < 8) return false;

    // Check for PNG signature
    if (data.size() >= 8 && data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') {
        // PNG-compressed icon - pass dimension limits to prevent large allocations
        decode_options png_opts;
        png_opts.max_width = max_w;
        png_opts.max_height = max_h;

        memory_surface temp;
        auto result = png_decoder::decode(data, temp, png_opts);
        if (!result) return false;

        icon.width = temp.width();
        icon.height = temp.height();
        icon.pixels.resize(static_cast<std::size_t>(icon.width) * static_cast<std::size_t>(icon.height) * 4);

        auto src = temp.pixels();
        if (temp.format() == pixel_format::rgba8888) {
            std::memcpy(icon.pixels.data(), src.data(), icon.pixels.size());
        } else if (temp.format() == pixel_format::rgb888) {
            for (int y = 0; y < icon.height; ++y) {
                for (int x = 0; x < icon.width; ++x) {
                    std::size_t src_idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(icon.width) + static_cast<std::size_t>(x)) * 3;
                    std::size_t dst_idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(icon.width) + static_cast<std::size_t>(x)) * 4;
                    icon.pixels[dst_idx + 0] = src[src_idx + 0];
                    icon.pixels[dst_idx + 1] = src[src_idx + 1];
                    icon.pixels[dst_idx + 2] = src[src_idx + 2];
                    icon.pixels[dst_idx + 3] = 0xFF;
                }
            }
        } else {
            return false;  // Unsupported format
        }
        return true;
    }

    // DIB format
    dib_header header;
    if (!parse_dib_header(data, header)) return false;

    // Validate compression - only uncompressed DIBs supported in ICO
    if (header.compression != BI_RGB) {
        return false;
    }

    // Validate bit_count - must be a valid value
    if (header.bit_count != 1 && header.bit_count != 4 && header.bit_count != 8 &&
        header.bit_count != 16 && header.bit_count != 24 && header.bit_count != 32) {
        return false;
    }

    // Icon height is doubled (includes AND mask) - must be even and positive
    std::int32_t abs_height = std::abs(header.height);
    if (abs_height < 2 || (abs_height % 2) != 0) {
        return false;  // Height must be even (XOR + AND masks)
    }

    icon.width = header.width;
    icon.height = abs_height / 2;

    if (icon.width <= 0 || icon.height <= 0 || icon.width > 256 || icon.height > 256) {
        return false;
    }

    // Calculate sizes
    std::size_t xor_stride = row_stride_4byte(icon.width, header.bit_count);
    std::size_t and_stride = row_stride_4byte(icon.width, 1);
    std::size_t xor_size = xor_stride * static_cast<std::size_t>(icon.height);
    std::size_t and_size = and_stride * static_cast<std::size_t>(icon.height);

    // Palette - only for indexed formats, with overflow protection
    std::uint32_t max_palette_colors = 0;
    if (header.bit_count <= 8) {
        max_palette_colors = 1u << header.bit_count;  // Safe: bit_count validated to be 1, 4, or 8
    }

    std::uint32_t palette_colors = header.clr_used;
    if (palette_colors == 0) {
        palette_colors = max_palette_colors;
    } else if (max_palette_colors > 0 && palette_colors > max_palette_colors) {
        // Clamp to maximum valid palette size
        palette_colors = max_palette_colors;
    }

    // Overflow-safe size calculations
    std::size_t palette_size = static_cast<std::size_t>(palette_colors) * 4;  // BGRA format

    // Check header.size is reasonable (prevent overflow in header_and_palette)
    if (header.size > data.size()) {
        return false;
    }

    std::size_t header_and_palette = static_cast<std::size_t>(header.size) + palette_size;

    // Check for overflow in header_and_palette
    if (header_and_palette < static_cast<std::size_t>(header.size)) {
        return false;  // Overflow occurred
    }

    // Check for overflow in total_needed
    if (header_and_palette > data.size() - xor_size ||
        (and_size > 0 && header_and_palette + xor_size > data.size() - and_size)) {
        // Try without AND mask
        if (header_and_palette > data.size() || xor_size > data.size() - header_and_palette) {
            return false;
        }
        and_size = 0;
    }

    const std::uint8_t* palette_ptr = data.data() + header.size;
    const std::uint8_t* xor_data = data.data() + header_and_palette;
    const std::uint8_t* and_data = and_size > 0 ? xor_data + xor_size : nullptr;

    // Allocate output
    icon.pixels.resize(static_cast<std::size_t>(icon.width) * static_cast<std::size_t>(icon.height) * 4);

    // Decode pixel data (bottom-up)
    for (int y = 0; y < icon.height; ++y) {
        int src_y = icon.height - 1 - y;  // DIB is bottom-up
        const std::uint8_t* src_row = xor_data + static_cast<std::size_t>(src_y) * xor_stride;

        for (int x = 0; x < icon.width; ++x) {
            std::size_t dst_idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(icon.width) + static_cast<std::size_t>(x)) * 4;
            std::uint8_t r = 0, g = 0, b = 0, a = 0xFF;

            if (header.bit_count <= 8) {
                // Indexed color
                std::uint8_t idx = extract_pixel(src_row, x, header.bit_count);
                if (idx < palette_colors) {
                    const std::uint8_t* pal = palette_ptr + idx * 4;
                    b = pal[0];
                    g = pal[1];
                    r = pal[2];
                }
            } else if (header.bit_count == 16) {
                // 16-bit (5-5-5 or 5-6-5)
                std::uint16_t pixel = read_le16(src_row + x * 2);
                r = static_cast<std::uint8_t>(((pixel >> 10) & 0x1F) << 3);
                g = static_cast<std::uint8_t>(((pixel >> 5) & 0x1F) << 3);
                b = static_cast<std::uint8_t>((pixel & 0x1F) << 3);
            } else if (header.bit_count == 24) {
                // 24-bit BGR
                const std::uint8_t* p = src_row + x * 3;
                b = p[0];
                g = p[1];
                r = p[2];
            } else if (header.bit_count == 32) {
                // 32-bit BGRA
                const std::uint8_t* p = src_row + x * 4;
                b = p[0];
                g = p[1];
                r = p[2];
                a = p[3];
            }

            // Apply AND mask for non-32bpp icons
            if (and_data && header.bit_count < 32) {
                if (get_and_mask_bit(and_data, icon.width, x, src_y)) {
                    a = 0;  // Transparent
                }
            }

            icon.pixels[dst_idx + 0] = r;
            icon.pixels[dst_idx + 1] = g;
            icon.pixels[dst_idx + 2] = b;
            icon.pixels[dst_idx + 3] = a;
        }
    }

    return true;
}

// Create atlas from multiple icons
decode_result create_icon_atlas(std::vector<decoded_icon>& icons, surface& surf,
                                 int max_w, int max_h) {
    if (icons.empty()) {
        return decode_result::failure(decode_error::invalid_format, "No valid icons");
    }

    // Calculate atlas dimensions (stack vertically) with overflow protection
    std::size_t atlas_width = 0;
    std::size_t atlas_height = 0;
    for (const auto& icon : icons) {
        atlas_width = std::max(atlas_width, static_cast<std::size_t>(icon.width));
        atlas_height += static_cast<std::size_t>(icon.height);

        // Early overflow check
        if (atlas_height > static_cast<std::size_t>(max_h)) {
            return decode_result::failure(decode_error::dimensions_exceeded,
                "ICO atlas height exceeds limits");
        }
    }

    // Check atlas dimensions against limits
    if (atlas_width > static_cast<std::size_t>(max_w)) {
        return decode_result::failure(decode_error::dimensions_exceeded,
            "ICO atlas width exceeds limits");
    }

    // Allocate surface
    if (!surf.set_size(static_cast<int>(atlas_width), static_cast<int>(atlas_height), pixel_format::rgba8888)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    // Copy icons to atlas
    int y_offset = 0;
    for (std::size_t i = 0; i < icons.size(); ++i) {
        const auto& icon = icons[i];

        // Copy each row
        for (int y = 0; y < icon.height; ++y) {
            const std::uint8_t* src_row = icon.pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(icon.width) * 4;
            surf.write_pixels(0, y_offset + y, icon.width * 4, src_row);
        }

        // Set subrect
        subrect sr;
        sr.rect = {0, y_offset, icon.width, icon.height};
        sr.kind = subrect_kind::sprite;
        sr.user_tag = static_cast<std::uint32_t>(i);
        surf.set_subrect(static_cast<int>(i), sr);

        y_offset += icon.height;
    }

    return decode_result::success();
}

}  // namespace

// ============================================================================
// ICO Decoder
// ============================================================================

bool ico_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    if (data.size() < 6) return false;
    ico_header header;
    return parse_ico_header(data, header) && header.count > 0;
}

decode_result ico_decoder::decode(std::span<const std::uint8_t> data,
                                   surface& surf,
                                   const decode_options& options) {
    ico_header header;
    if (!parse_ico_header(data, header)) {
        return decode_result::failure(decode_error::invalid_format, "Invalid ICO header");
    }

    if (header.count == 0) {
        return decode_result::failure(decode_error::invalid_format, "ICO file has no images");
    }

    // Check dimension limits
    const int max_w = options.max_width > 0 ? options.max_width : 256;
    const int max_h = options.max_height > 0 ? options.max_height : 256;

    // Parse directory entries
    std::vector<ico_dir_entry> entries;
    entries.reserve(header.count);

    std::size_t dir_offset = 6;
    for (int i = 0; i < header.count; ++i) {
        if (dir_offset + 16 > data.size()) break;

        ico_dir_entry entry;
        parse_ico_dir_entry(data.data() + dir_offset, entry);
        dir_offset += 16;

        // Validate entry
        int w = entry.width == 0 ? 256 : entry.width;
        int h = entry.height == 0 ? 256 : entry.height;
        if (w <= max_w && h <= max_h && entry.offset < data.size() && entry.size > 0) {
            entries.push_back(entry);
        }
    }

    if (entries.empty()) {
        return decode_result::failure(decode_error::invalid_format, "No valid icon entries");
    }

    // Decode each icon
    std::vector<decoded_icon> icons;
    icons.reserve(entries.size());

    for (const auto& entry : entries) {
        if (entry.offset + entry.size > data.size()) continue;

        auto icon_data = data.subspan(entry.offset, entry.size);
        decoded_icon icon;
        if (decode_icon_image(icon_data, icon, max_w, max_h)) {
            icons.push_back(std::move(icon));
        }
    }

    return create_icon_atlas(icons, surf, max_w, max_h);
}

// ============================================================================
// EXE Icon Decoder
// ============================================================================

bool exe_icon_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    if (data.size() < 64) return false;

    // Check for MZ signature
    if (data[0] != 'M' || data[1] != 'Z') return false;

    // Check for extended header (NE/PE/LX)
    auto format = libexe::executable_factory::detect_format(data);
    return format == libexe::format_type::NE_WIN16 ||
           format == libexe::format_type::PE_WIN32 ||
           format == libexe::format_type::PE_PLUS_WIN64 ||
           format == libexe::format_type::LX_OS2_BOUND ||
           format == libexe::format_type::LX_OS2_RAW;
}

decode_result exe_icon_decoder::decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) {
    // Check dimension limits
    const int max_w = options.max_width > 0 ? options.max_width : 256;
    const int max_h = options.max_height > 0 ? options.max_height : 256;

    std::vector<decoded_icon> icons;

    try {
        auto exe = libexe::executable_factory::from_memory(data);

        std::visit([&icons, &options, max_w, max_h](auto& file) {
            using T = std::decay_t<decltype(file)>;

            if constexpr (std::is_same_v<T, libexe::ne_file> || std::is_same_v<T, libexe::pe_file>) {
                // NE/PE: Use resource_directory API
                auto resources = file.resources();
                if (!resources) return;

                auto icon_resources = resources->resources_by_type(libexe::resource_type::RT_ICON);
                icons.reserve(icon_resources.size());

                for (std::size_t i = 0; i < icon_resources.size(); ++i) {
                    auto icon_res = icon_resources.at(i);
                    if (!icon_res) continue;

                    auto icon_image = icon_res->as_icon();
                    if (!icon_image) continue;

                    auto dib_data = icon_image->raw_dib_data();
                    decoded_icon icon;
                    if (decode_icon_image(dib_data, icon, max_w, max_h)) {
                        icons.push_back(std::move(icon));
                    }
                }
            } else if constexpr (std::is_same_v<T, libexe::le_file>) {
                // LX/LE: Use le_resource API (OS/2 uses RT_POINTER for icons)
                if (!file.has_resources()) return;

                // Try RT_POINTER (OS/2 icon format) - type ID 1
                auto pointer_resources = file.resources_by_type(libexe::le_resource::RT_POINTER);
                icons.reserve(pointer_resources.size());

                for (const auto& res : pointer_resources) {
                    auto res_data = file.read_resource_data(res);
                    if (res_data.empty()) continue;

                    decoded_icon icon;
                    if (decode_icon_image(res_data, icon, max_w, max_h)) {
                        icons.push_back(std::move(icon));
                    }
                }
            }
        }, exe);
    } catch (const std::exception& e) {
        return decode_result::failure(decode_error::invalid_format, e.what());
    }

    if (icons.empty()) {
        return decode_result::failure(decode_error::invalid_format, "No icons in executable");
    }

    return create_icon_atlas(icons, surf, max_w, max_h);
}

}  // namespace onyx_image
