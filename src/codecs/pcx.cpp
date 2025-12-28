#include <onyx_image/codecs/pcx.hpp>
#include <formats/pcx/pcx.hh>

#include <algorithm>
#include <cstring>

namespace onyx_image {

namespace {

constexpr std::size_t PCX_HEADER_SIZE = 128;
constexpr std::size_t VGA_PALETTE_SIZE = 769;  // 1 marker + 768 color bytes
constexpr std::uint8_t RLE_MASK = 0xC0;
constexpr std::uint8_t RLE_COUNT_MASK = 0x3F;

} // namespace

bool pcx_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    if (data.size() < PCX_HEADER_SIZE) {
        return false;
    }

    // Check signature byte
    if (data[0] != formats::pcx::PCX_SIGNATURE) {
        return false;
    }

    // Check version (valid values: 0, 2, 3, 4, 5)
    const auto version = data[1];
    if (version != 0 && version != 2 && version != 3 && version != 4 && version != 5) {
        return false;
    }

    // Check encoding (0 or 1)
    if (data[2] > 1) {
        return false;
    }

    // Check bits per pixel (1, 2, 4, 8)
    const auto bpp = data[3];
    if (bpp != 1 && bpp != 2 && bpp != 4 && bpp != 8) {
        return false;
    }

    return true;
}

decode_result pcx_decoder::parse_header(std::span<const std::uint8_t> data,
                                         header_info& info,
                                         const decode_options& options) {
    if (data.size() < PCX_HEADER_SIZE) {
        return decode_result::failure(decode_error::truncated_data,
            "PCX file too small: expected at least 128 bytes");
    }

    try {
        const auto* ptr = data.data();
        const auto* end = ptr + data.size();
        auto header = formats::pcx::pcx_header::read(ptr, end);

        info.version = header.version;
        info.bits_per_pixel = header.bits_per_pixel;
        info.num_planes = header.num_planes;
        info.bytes_per_line = header.bytes_per_line;
        info.has_rle = (header.encoding == formats::pcx::PCX_ENCODING_RLE);

        // Calculate dimensions
        info.width = header.x_max - header.x_min + 1;
        info.height = header.y_max - header.y_min + 1;

        // Validate dimensions
        if (info.width <= 0 || info.height <= 0) {
            return decode_result::failure(decode_error::invalid_format, "Invalid image dimensions");
        }

        const int max_w = options.max_width > 0 ? options.max_width : 16384;
        const int max_h = options.max_height > 0 ? options.max_height : 16384;

        if (info.width > max_w || info.height > max_h) {
            return decode_result::failure(decode_error::dimensions_exceeded,
                "Image dimensions exceed limits");
        }

        // Validate bits per pixel
        if (info.bits_per_pixel != 1 && info.bits_per_pixel != 2 &&
            info.bits_per_pixel != 4 && info.bits_per_pixel != 8) {
            return decode_result::failure(decode_error::unsupported_bit_depth,
                "Unsupported bits per pixel");
        }

        // Validate planes (1, 2, 3, or 4 planes are valid)
        if (info.num_planes < 1 || info.num_planes > 4) {
            return decode_result::failure(decode_error::unsupported_encoding,
                "Unsupported number of color planes");
        }

        return decode_result::success();

    } catch (const formats::pcx::ConstraintViolation& e) {
        return decode_result::failure(decode_error::invalid_format, e.what());
    } catch (const std::exception& e) {
        return decode_result::failure(decode_error::internal_error, e.what());
    }
}

decode_result pcx_decoder::decode_rle(std::span<const std::uint8_t> data,
                                       std::size_t data_offset,
                                       const header_info& info,
                                       surface& surf) {
    const std::size_t scan_line_length = static_cast<std::size_t>(info.bytes_per_line) * info.num_planes;
    std::vector<std::uint8_t> scan_line(scan_line_length);

    const auto* src = data.data() + data_offset;
    const auto* src_end = data.data() + data.size();

    // For VGA palette, don't read into the palette area
    if (info.version == 5 && info.bits_per_pixel == 8 && info.num_planes == 1) {
        if (data.size() >= VGA_PALETTE_SIZE) {
            src_end = data.data() + data.size() - VGA_PALETTE_SIZE;
        }
    }

    for (int y = 0; y < info.height; ++y) {
        std::size_t line_pos = 0;

        // Decode one scan line
        while (line_pos < scan_line_length && src < src_end) {
            std::uint8_t byte = *src++;

            if ((byte & RLE_MASK) == RLE_MASK) {
                // RLE run
                int count = byte & RLE_COUNT_MASK;
                if (src >= src_end) {
                    return decode_result::failure(decode_error::truncated_data,
                        "PCX data truncated: incomplete RLE sequence");
                }
                std::uint8_t value = *src++;

                // Validate count won't exceed scanline length
                std::size_t remaining = scan_line_length - line_pos;
                std::size_t to_write = std::min(static_cast<std::size_t>(count), remaining);
                for (std::size_t i = 0; i < to_write; ++i) {
                    scan_line[line_pos++] = value;
                }
            } else {
                // Literal byte
                scan_line[line_pos++] = byte;
            }
        }

        // Check for truncated scanline
        if (line_pos < scan_line_length) {
            return decode_result::failure(decode_error::truncated_data,
                "Truncated PCX scanline");
        }

        // Convert scan line to output format
        if (info.num_planes == 1 && info.bits_per_pixel == 8) {
            // 256-color indexed - direct copy
            surf.write_pixels(0, y, info.width, scan_line.data());
        } else if (info.num_planes == 3 && info.bits_per_pixel == 8) {
            // 24-bit RGB - interleave planes
            for (int x = 0; x < info.width; ++x) {
                std::uint8_t rgb[3] = {
                    scan_line[x],                                    // R
                    scan_line[x + info.bytes_per_line],              // G
                    scan_line[x + info.bytes_per_line * 2]           // B
                };
                surf.write_pixels(x * 3, y, 3, rgb);
            }
        } else if (info.num_planes == 1 && info.bits_per_pixel == 1) {
            // Monochrome - expand bits to bytes
            for (int x = 0; x < info.width; ++x) {
                int byte_idx = x / 8;
                int bit_idx = 7 - (x % 8);
                std::uint8_t pixel = (scan_line[byte_idx] >> bit_idx) & 1;
                surf.write_pixel(x, y, pixel);
            }
        } else if (info.num_planes == 1 && info.bits_per_pixel == 4) {
            // 16-color - expand nibbles
            for (int x = 0; x < info.width; ++x) {
                int byte_idx = x / 2;
                std::uint8_t pixel;
                if (x % 2 == 0) {
                    pixel = (scan_line[byte_idx] >> 4) & 0x0F;
                } else {
                    pixel = scan_line[byte_idx] & 0x0F;
                }
                surf.write_pixel(x, y, pixel);
            }
        } else if (info.num_planes == 4 && info.bits_per_pixel == 1) {
            // EGA 16-color planar (4 planes)
            for (int x = 0; x < info.width; ++x) {
                int byte_idx = x / 8;
                int bit_idx = 7 - (x % 8);
                std::uint8_t pixel = 0;
                for (int plane = 0; plane < 4; ++plane) {
                    std::uint8_t bit = (scan_line[byte_idx + plane * info.bytes_per_line] >> bit_idx) & 1;
                    pixel |= (bit << plane);
                }
                surf.write_pixel(x, y, pixel);
            }
        } else if (info.num_planes == 3 && info.bits_per_pixel == 1) {
            // EGA 8-color planar (3 planes)
            for (int x = 0; x < info.width; ++x) {
                int byte_idx = x / 8;
                int bit_idx = 7 - (x % 8);
                std::uint8_t pixel = 0;
                for (int plane = 0; plane < 3; ++plane) {
                    std::uint8_t bit = (scan_line[byte_idx + plane * info.bytes_per_line] >> bit_idx) & 1;
                    pixel |= (bit << plane);
                }
                surf.write_pixel(x, y, pixel);
            }
        } else if (info.num_planes == 2 && info.bits_per_pixel == 1) {
            // CGA 4-color planar (2 planes)
            for (int x = 0; x < info.width; ++x) {
                int byte_idx = x / 8;
                int bit_idx = 7 - (x % 8);
                std::uint8_t pixel = 0;
                for (int plane = 0; plane < 2; ++plane) {
                    std::uint8_t bit = (scan_line[byte_idx + plane * info.bytes_per_line] >> bit_idx) & 1;
                    pixel |= (bit << plane);
                }
                surf.write_pixel(x, y, pixel);
            }
        } else if (info.num_planes == 1 && info.bits_per_pixel == 2) {
            // CGA 4-color packed (2 bits per pixel)
            for (int x = 0; x < info.width; ++x) {
                int byte_idx = x / 4;
                int shift = 6 - (x % 4) * 2;
                std::uint8_t pixel = (scan_line[byte_idx] >> shift) & 0x03;
                surf.write_pixel(x, y, pixel);
            }
        } else {
            return decode_result::failure(decode_error::unsupported_encoding,
                "Unsupported PCX format combination");
        }
    }

    return decode_result::success();
}

void pcx_decoder::apply_ega_palette(std::span<const std::uint8_t> header_data,
                                     surface& surf) {
    // EGA palette is at offset 16 in header, 48 bytes (16 colors * 3)
    surf.set_palette_size(16);
    surf.write_palette(0, header_data.subspan(16, 48));
}

void pcx_decoder::apply_cga_palette(std::span<const std::uint8_t> header_data,
                                     surface& surf) {
    // CGA 4-color palette handling
    // Background color index is in upper nibble of byte 16 (per dr_pcx)
    // Palette selector is at offset 19:
    //   Bit 5: Palette (0 = cyan/magenta/white, 1 = green/red/brown)
    //   Bit 4: Intensity inverted (0 = high, 1 = low)

    // CGA 16-color palette for background
    static constexpr std::uint8_t CGA_16_COLORS[16][3] = {
        {0, 0, 0},       // 0: Black
        {0, 0, 170},     // 1: Blue
        {0, 170, 0},     // 2: Green
        {0, 170, 170},   // 3: Cyan
        {170, 0, 0},     // 4: Red
        {170, 0, 170},   // 5: Magenta
        {170, 85, 0},    // 6: Brown
        {170, 170, 170}, // 7: Light Gray
        {85, 85, 85},    // 8: Dark Gray
        {85, 85, 255},   // 9: Light Blue
        {85, 255, 85},   // 10: Light Green
        {85, 255, 255},  // 11: Light Cyan
        {255, 85, 85},   // 12: Light Red
        {255, 85, 255},  // 13: Light Magenta
        {255, 255, 85},  // 14: Yellow
        {255, 255, 255}, // 15: White
    };

    // CGA 4-color palette colors (foreground colors 1-3)
    // Palette 0: cyan, magenta, white/light gray
    // Palette 1: green, red, brown/yellow
    static constexpr std::uint8_t CGA_PALETTES[2][2][3][3] = {
        // Palette 0 (cyan/magenta/white)
        {
            // Low intensity
            {{0, 170, 170}, {170, 0, 170}, {170, 170, 170}},
            // High intensity
            {{85, 255, 255}, {255, 85, 255}, {255, 255, 255}}
        },
        // Palette 1 (green/red/brown)
        {
            // Low intensity
            {{0, 170, 0}, {170, 0, 0}, {170, 85, 0}},
            // High intensity
            {{85, 255, 85}, {255, 85, 85}, {255, 255, 85}}
        }
    };

    const std::uint8_t selector = header_data[19];
    const int palette = (selector >> 5) & 1;
    const int intensity = 1 - ((selector >> 4) & 1);  // Inverted

    // Background color: use CGA 16-color palette index from upper nibble of byte 16
    const int bg_index = header_data[16] >> 4;

    std::vector<std::uint8_t> pal(12);  // 4 colors * 3 bytes

    // Color 0: background from CGA 16-color palette
    pal[0] = CGA_16_COLORS[bg_index][0];
    pal[1] = CGA_16_COLORS[bg_index][1];
    pal[2] = CGA_16_COLORS[bg_index][2];

    // Colors 1-3: from CGA 4-color palette
    for (int i = 0; i < 3; ++i) {
        pal[(i + 1) * 3 + 0] = CGA_PALETTES[palette][intensity][i][0];
        pal[(i + 1) * 3 + 1] = CGA_PALETTES[palette][intensity][i][1];
        pal[(i + 1) * 3 + 2] = CGA_PALETTES[palette][intensity][i][2];
    }

    surf.set_palette_size(4);
    surf.write_palette(0, pal);
}

decode_result pcx_decoder::apply_vga_palette(std::span<const std::uint8_t> data,
                                              surface& surf) {
    if (data.size() < VGA_PALETTE_SIZE) {
        return decode_result::failure(decode_error::truncated_data,
            "PCX file too small: missing VGA palette");
    }

    const std::size_t palette_offset = data.size() - VGA_PALETTE_SIZE;
    const std::uint8_t marker = data[palette_offset];

    if (marker != formats::pcx::PCX_VGA_PALETTE_MARKER) {
        // No VGA palette, use default grayscale
        std::vector<std::uint8_t> palette(768);
        for (int i = 0; i < 256; ++i) {
            palette[i * 3 + 0] = static_cast<std::uint8_t>(i);
            palette[i * 3 + 1] = static_cast<std::uint8_t>(i);
            palette[i * 3 + 2] = static_cast<std::uint8_t>(i);
        }
        surf.set_palette_size(256);
        surf.write_palette(0, palette);
        return decode_result::success();
    }

    surf.set_palette_size(256);
    surf.write_palette(0, data.subspan(palette_offset + 1, 768));
    return decode_result::success();
}

decode_result pcx_decoder::decode(std::span<const std::uint8_t> data,
                                   surface& surf,
                                   const decode_options& options) {
    if (!sniff(data)) {
        return decode_result::failure(decode_error::invalid_format, "Not a valid PCX file");
    }

    header_info info{};
    auto result = parse_header(data, info, options);
    if (!result) {
        return result;
    }

    // Determine output format
    pixel_format fmt;
    if (info.num_planes == 3 && info.bits_per_pixel == 8) {
        fmt = pixel_format::rgb888;
    } else {
        fmt = pixel_format::indexed8;
    }

    // Allocate surface
    if (!surf.set_size(info.width, info.height, fmt)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    // Decode pixel data
    result = decode_rle(data, PCX_HEADER_SIZE, info, surf);
    if (!result) {
        return result;
    }

    // Apply palette for indexed formats
    if (fmt == pixel_format::indexed8) {
        if (info.version == 5 && info.bits_per_pixel == 8 && info.num_planes == 1) {
            // VGA 256-color palette at end of file
            result = apply_vga_palette(data, surf);
            if (!result) {
                return result;
            }
        } else if (info.bits_per_pixel == 2 && info.num_planes == 1) {
            // CGA 4-color mode - special palette handling
            apply_cga_palette(data, surf);
        } else {
            // EGA palette in header
            apply_ega_palette(data, surf);
        }
    }

    return decode_result::success();
}

} // namespace onyx_image
