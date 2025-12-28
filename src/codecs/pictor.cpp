#include <onyx_image/codecs/pictor.hpp>
#include "byte_io.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace onyx_image {

namespace {

// PICTOR constants
constexpr std::uint16_t PICTOR_MAGIC = 0x1234;
constexpr std::uint16_t PAL_NONE = 0;
constexpr std::uint16_t PAL_CGA = 1;
constexpr std::uint16_t PAL_PCJR = 2;
constexpr std::uint16_t PAL_EGA = 3;
constexpr std::uint16_t PAL_VGA = 4;

struct pic_info {
    int width = 0;
    int height = 0;
    int bits_per_pixel = 0;
    int num_planes = 0;
    std::uint16_t palette_type = 0;
    std::uint16_t palette_size = 0;
    std::uint8_t video_mode = 0;
};

bool parse_header(std::span<const std::uint8_t> data, pic_info& info) {
    if (data.size() < 17) {
        return false;
    }

    const std::uint8_t* ptr = data.data();

    std::uint16_t magic = read_le16(ptr);
    if (magic != PICTOR_MAGIC) {
        return false;
    }

    info.width = static_cast<int>(read_le16(ptr + 2));
    info.height = static_cast<int>(read_le16(ptr + 4));
    // x_offset and y_offset at ptr + 6 and ptr + 8 (ignored)

    std::uint8_t plane_info = ptr[10];
    info.bits_per_pixel = (plane_info & 0x0F);
    info.num_planes = ((plane_info >> 4) & 0x0F) + 1;

    // palette_flag at ptr[11] (0xFF = version 2.0+ with palette)
    info.video_mode = ptr[12];
    info.palette_type = read_le16(ptr + 13);
    info.palette_size = read_le16(ptr + 15);

    return true;
}

// Decode RLE block
// Returns number of bytes consumed from src
std::size_t decode_rle_block(
    const std::uint8_t* src, std::size_t src_size,
    std::vector<std::uint8_t>& dest, std::size_t max_pixels)
{
    if (src_size < 5) {
        return 0;
    }

    std::uint16_t block_size = read_le16(src);
    std::uint16_t run_length = read_le16(src + 2);
    std::uint8_t run_marker = src[4];

    if (block_size < 5 || block_size > src_size) {
        return 0;
    }

    const std::uint8_t* block_data = src + 5;
    const std::uint8_t* block_end = src + block_size;
    std::size_t pixels_decoded = 0;

    while (block_data < block_end && pixels_decoded < run_length && dest.size() < max_pixels) {
        std::uint8_t byte = *block_data++;

        if (byte == run_marker) {
            if (block_data >= block_end) break;
            std::uint8_t count_byte = *block_data++;

            if (count_byte == 0) {
                // Extended run: 16-bit count follows
                if (block_data + 2 > block_end) break;
                std::uint16_t count = read_le16(block_data);
                block_data += 2;
                if (block_data >= block_end) break;
                std::uint8_t value = *block_data++;

                for (std::uint16_t i = 0; i < count && dest.size() < max_pixels; i++) {
                    dest.push_back(value);
                    pixels_decoded++;
                }
            } else {
                // Short run: count is the byte value directly (not +1)
                if (block_data >= block_end) break;
                std::uint8_t value = *block_data++;
                std::size_t count = static_cast<std::size_t>(count_byte);

                for (std::size_t i = 0; i < count && dest.size() < max_pixels; i++) {
                    dest.push_back(value);
                    pixels_decoded++;
                }
            }
        } else {
            // Literal byte
            dest.push_back(byte);
            pixels_decoded++;
        }
    }

    return block_size;
}

// Full 64-color EGA palette (6-bit RGB, 2 bits per component)
// From FFmpeg's ff_ega_palette
const std::uint32_t ega_palette_64[64] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAAAA00, 0xAAAAAA,
    0x000055, 0x0000FF, 0x00AA55, 0x00AAFF, 0xAA0055, 0xAA00FF, 0xAAAA55, 0xAAAAFF,
    0x005500, 0x0055AA, 0x00FF00, 0x00FFAA, 0xAA5500, 0xAA55AA, 0xAAFF00, 0xAAFFAA,
    0x005555, 0x0055FF, 0x00FF55, 0x00FFFF, 0xAA5555, 0xAA55FF, 0xAAFF55, 0xAAFFFF,
    0x550000, 0x5500AA, 0x55AA00, 0x55AAAA, 0xFF0000, 0xFF00AA, 0xFFAA00, 0xFFAAAA,
    0x550055, 0x5500FF, 0x55AA55, 0x55AAFF, 0xFF0055, 0xFF00FF, 0xFFAA55, 0xFFAAFF,
    0x555500, 0x5555AA, 0x55FF00, 0x55FFAA, 0xFF5500, 0xFF55AA, 0xFFFF00, 0xFFFFAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
};

// 16-color CGA palette (matches ff_cga_palette)
const std::uint32_t cga_palette_16[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
};

// CGA mode 4/5 palette index table (6 variants)
const std::uint8_t cga_mode45_index[6][4] = {
    { 0, 3,  5,   7 }, // mode4, palette#1, low intensity
    { 0, 2,  4,   6 }, // mode4, palette#2, low intensity
    { 0, 3,  4,   7 }, // mode5, low intensity
    { 0, 11, 13, 15 }, // mode4, palette#1, high intensity
    { 0, 10, 12, 14 }, // mode4, palette#2, high intensity
    { 0, 11, 12, 15 }, // mode5, high intensity
};

} // namespace

bool pictor_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    if (data.size() < 2) {
        return false;
    }
    // Check little-endian magic number: 0x1234
    return data[0] == 0x34 && data[1] == 0x12;
}

decode_result pictor_decoder::decode(std::span<const std::uint8_t> data,
                                      surface& surf,
                                      const decode_options& options) {
    if (!sniff(data)) {
        return decode_result::failure(decode_error::invalid_format, "Not a valid PICTOR file");
    }

    pic_info info;
    if (!parse_header(data, info)) {
        return decode_result::failure(decode_error::invalid_format, "Failed to parse PICTOR header");
    }

    if (info.width <= 0 || info.height <= 0) {
        return decode_result::failure(decode_error::invalid_format, "Invalid image dimensions");
    }

    // Check dimension limits
    const int max_w = options.max_width > 0 ? options.max_width : 16384;
    const int max_h = options.max_height > 0 ? options.max_height : 16384;
    if (info.width > max_w || info.height > max_h) {
        return decode_result::failure(decode_error::dimensions_exceeded, "Image dimensions exceed limits");
    }

    // Calculate effective bits per pixel
    int total_bpp = info.bits_per_pixel * info.num_planes;

    // Validate supported modes
    if (info.num_planes == 1) {
        if (info.bits_per_pixel != 1 && info.bits_per_pixel != 2 &&
            info.bits_per_pixel != 4 && info.bits_per_pixel != 8) {
            return decode_result::failure(decode_error::invalid_format,
                "Unsupported bits per pixel: " + std::to_string(info.bits_per_pixel));
        }
    } else if (info.num_planes == 4) {
        if (info.bits_per_pixel != 1) {
            return decode_result::failure(decode_error::invalid_format,
                "Unsupported planar format: " + std::to_string(info.bits_per_pixel) +
                " bpp x " + std::to_string(info.num_planes) + " planes");
        }
    } else {
        return decode_result::failure(decode_error::invalid_format,
            "Unsupported number of planes: " + std::to_string(info.num_planes));
    }

    // Calculate data positions
    const std::size_t header_size = 17;
    const std::size_t palette_offset = header_size;
    const std::size_t pixel_offset = header_size + info.palette_size;

    if (pixel_offset + 2 > data.size()) {
        return decode_result::failure(decode_error::truncated_data,
            "PICTOR data truncated: incomplete file header");
    }

    // Read palette
    std::vector<std::uint8_t> palette;
    int num_colors = 0;

    if (total_bpp <= 8) {
        num_colors = 1 << total_bpp;
        palette.resize(static_cast<std::size_t>(num_colors) * 3);

        if (info.palette_type == PAL_VGA && info.palette_size >= 768) {
            // VGA palette: 256 RGB triplets with 6-bit values
            // Scale 6-bit to 8-bit: replicate high bits to low bits for accurate conversion
            const std::uint8_t* pal_data = data.data() + palette_offset;
            for (int i = 0; i < 256 && i < num_colors; i++) {
                std::uint8_t r = pal_data[i * 3 + 0];
                std::uint8_t g = pal_data[i * 3 + 1];
                std::uint8_t b = pal_data[i * 3 + 2];
                palette[static_cast<std::size_t>(i) * 3 + 0] = static_cast<std::uint8_t>((r << 2) | (r >> 4));
                palette[static_cast<std::size_t>(i) * 3 + 1] = static_cast<std::uint8_t>((g << 2) | (g >> 4));
                palette[static_cast<std::size_t>(i) * 3 + 2] = static_cast<std::uint8_t>((b << 2) | (b >> 4));
            }
        } else if (info.palette_type == PAL_EGA && info.palette_size >= 16) {
            // EGA palette: 16 bytes, each byte indexes into 64-color EGA palette
            const std::uint8_t* pal_data = data.data() + palette_offset;
            for (int i = 0; i < 16 && i < num_colors; i++) {
                std::uint8_t idx = pal_data[i] & 0x3F;  // 6-bit index into 64-color palette
                std::uint32_t color = ega_palette_64[idx];
                palette[static_cast<std::size_t>(i) * 3 + 0] = static_cast<std::uint8_t>((color >> 16) & 0xFF);
                palette[static_cast<std::size_t>(i) * 3 + 1] = static_cast<std::uint8_t>((color >> 8) & 0xFF);
                palette[static_cast<std::size_t>(i) * 3 + 2] = static_cast<std::uint8_t>(color & 0xFF);
            }
        } else if (info.palette_type == PAL_CGA && info.palette_size >= 1) {
            // CGA palette: first byte is index into cga_mode45_index table
            const std::uint8_t* pal_data = data.data() + palette_offset;
            int idx = pal_data[0];
            if (idx >= 6) idx = 0;  // Clamp to valid range

            // Use selected CGA palette variant
            for (int i = 0; i < 4 && i < num_colors; i++) {
                std::uint8_t color_idx = cga_mode45_index[idx][i];
                std::uint32_t color = cga_palette_16[color_idx];
                palette[static_cast<std::size_t>(i) * 3 + 0] = static_cast<std::uint8_t>((color >> 16) & 0xFF);
                palette[static_cast<std::size_t>(i) * 3 + 1] = static_cast<std::uint8_t>((color >> 8) & 0xFF);
                palette[static_cast<std::size_t>(i) * 3 + 2] = static_cast<std::uint8_t>(color & 0xFF);
            }
        } else if (info.palette_size >= static_cast<std::size_t>(num_colors) * 3) {
            // Generic RGB palette with 6-bit values (like VGA but for any color count)
            const std::uint8_t* pal_data = data.data() + palette_offset;
            for (int i = 0; i < num_colors; i++) {
                std::uint8_t r = pal_data[i * 3 + 0];
                std::uint8_t g = pal_data[i * 3 + 1];
                std::uint8_t b = pal_data[i * 3 + 2];
                palette[static_cast<std::size_t>(i) * 3 + 0] = static_cast<std::uint8_t>((r << 2) | (r >> 4));
                palette[static_cast<std::size_t>(i) * 3 + 1] = static_cast<std::uint8_t>((g << 2) | (g >> 4));
                palette[static_cast<std::size_t>(i) * 3 + 2] = static_cast<std::uint8_t>((b << 2) | (b >> 4));
            }
        } else if (num_colors == 2) {
            // Monochrome: black and white
            palette[0] = palette[1] = palette[2] = 0x00;
            palette[3] = palette[4] = palette[5] = 0xFF;
        } else if (num_colors == 4) {
            // Default CGA palette (mode4, palette#1, low intensity)
            for (int i = 0; i < 4; i++) {
                std::uint8_t color_idx = cga_mode45_index[0][i];
                std::uint32_t color = cga_palette_16[color_idx];
                palette[static_cast<std::size_t>(i) * 3 + 0] = static_cast<std::uint8_t>((color >> 16) & 0xFF);
                palette[static_cast<std::size_t>(i) * 3 + 1] = static_cast<std::uint8_t>((color >> 8) & 0xFF);
                palette[static_cast<std::size_t>(i) * 3 + 2] = static_cast<std::uint8_t>(color & 0xFF);
            }
        } else if (num_colors == 16) {
            // Default EGA palette (first 16 entries from 64-color palette)
            for (int i = 0; i < 16; i++) {
                std::uint32_t color = ega_palette_64[i];
                palette[static_cast<std::size_t>(i) * 3 + 0] = static_cast<std::uint8_t>((color >> 16) & 0xFF);
                palette[static_cast<std::size_t>(i) * 3 + 1] = static_cast<std::uint8_t>((color >> 8) & 0xFF);
                palette[static_cast<std::size_t>(i) * 3 + 2] = static_cast<std::uint8_t>(color & 0xFF);
            }
        } else {
            // Grayscale
            for (int i = 0; i < num_colors; i++) {
                std::uint8_t gray = static_cast<std::uint8_t>((i * 255) / (num_colors - 1));
                palette[static_cast<std::size_t>(i) * 3 + 0] = gray;
                palette[static_cast<std::size_t>(i) * 3 + 1] = gray;
                palette[static_cast<std::size_t>(i) * 3 + 2] = gray;
            }
        }
    }

    // Read RLE block count
    const std::uint8_t* pixel_ptr = data.data() + pixel_offset;
    const std::uint8_t* data_end = data.data() + data.size();

    if (pixel_ptr + 2 > data_end) {
        return decode_result::failure(decode_error::truncated_data, "Missing RLE block count");
    }

    std::uint16_t block_count = read_le16(pixel_ptr);
    pixel_ptr += 2;

    // Calculate expected raw pixel data size
    std::size_t row_bytes = (static_cast<std::size_t>(info.width) * static_cast<std::size_t>(info.bits_per_pixel) + 7) / 8;
    std::size_t plane_size = row_bytes * static_cast<std::size_t>(info.height);
    std::size_t total_size = plane_size * static_cast<std::size_t>(info.num_planes);

    // Decompress all RLE blocks
    std::vector<std::uint8_t> decompressed;
    decompressed.reserve(total_size);

    if (block_count == 0) {
        // Uncompressed data
        std::size_t avail = static_cast<std::size_t>(data_end - pixel_ptr);
        std::size_t to_copy = std::min(avail, total_size);
        decompressed.assign(pixel_ptr, pixel_ptr + to_copy);
    } else {
        // RLE compressed
        for (std::uint16_t b = 0; b < block_count && pixel_ptr < data_end; b++) {
            std::size_t consumed = decode_rle_block(
                pixel_ptr,
                static_cast<std::size_t>(data_end - pixel_ptr),
                decompressed,
                total_size
            );
            if (consumed == 0) break;
            pixel_ptr += consumed;
        }
    }

    // Pad to expected size
    while (decompressed.size() < total_size) {
        decompressed.push_back(0);
    }

    // Determine output format
    pixel_format out_format = pixel_format::indexed8;
    if (!surf.set_size(info.width, info.height, out_format)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    // Set palette
    if (!palette.empty()) {
        surf.set_palette_size(num_colors);
        surf.write_palette(0, palette);
    }

    // Decode pixels
    // PICTOR stores scanlines bottom-up, so flip when writing to output
    std::vector<std::uint8_t> row_buffer(static_cast<std::size_t>(info.width));

    if (info.num_planes == 1) {
        // Single plane mode
        for (int y = 0; y < info.height; y++) {
            const std::uint8_t* src_row = decompressed.data() + static_cast<std::size_t>(y) * row_bytes;
            int dest_y = info.height - 1 - y;  // Flip to top-down

            if (info.bits_per_pixel == 8) {
                // 8-bit: direct copy
                std::memcpy(row_buffer.data(), src_row, static_cast<std::size_t>(info.width));
            } else if (info.bits_per_pixel == 4) {
                // 4-bit: high nibble first
                for (int x = 0; x < info.width; x++) {
                    std::size_t byte_idx = static_cast<std::size_t>(x) / 2;
                    if (x % 2 == 0) {
                        row_buffer[static_cast<std::size_t>(x)] = (src_row[byte_idx] >> 4) & 0x0F;
                    } else {
                        row_buffer[static_cast<std::size_t>(x)] = src_row[byte_idx] & 0x0F;
                    }
                }
            } else if (info.bits_per_pixel == 2) {
                // 2-bit: 4 pixels per byte, MSB first
                for (int x = 0; x < info.width; x++) {
                    std::size_t byte_idx = static_cast<std::size_t>(x) / 4;
                    int shift = 6 - (x % 4) * 2;
                    row_buffer[static_cast<std::size_t>(x)] = (src_row[byte_idx] >> shift) & 0x03;
                }
            } else if (info.bits_per_pixel == 1) {
                // 1-bit: 8 pixels per byte, MSB first
                for (int x = 0; x < info.width; x++) {
                    std::size_t byte_idx = static_cast<std::size_t>(x) / 8;
                    int bit_idx = 7 - (x % 8);
                    row_buffer[static_cast<std::size_t>(x)] = (src_row[byte_idx] >> bit_idx) & 0x01;
                }
            }

            surf.write_pixels(0, dest_y, info.width, row_buffer.data());
        }
    } else if (info.num_planes == 4 && info.bits_per_pixel == 1) {
        // EGA planar mode: 4 planes, 1 bit per pixel per plane
        // Each plane is stored contiguously, then combined to get 4-bit color index
        for (int y = 0; y < info.height; y++) {
            std::fill(row_buffer.begin(), row_buffer.end(), 0);
            int dest_y = info.height - 1 - y;  // Flip to top-down

            for (int plane = 0; plane < 4; plane++) {
                const std::uint8_t* plane_row = decompressed.data() +
                    static_cast<std::size_t>(plane) * plane_size +
                    static_cast<std::size_t>(y) * row_bytes;

                for (int x = 0; x < info.width; x++) {
                    std::size_t byte_idx = static_cast<std::size_t>(x) / 8;
                    int bit_idx = 7 - (x % 8);
                    std::uint8_t bit = (plane_row[byte_idx] >> bit_idx) & 0x01;
                    row_buffer[static_cast<std::size_t>(x)] |= static_cast<std::uint8_t>(bit << plane);
                }
            }

            surf.write_pixels(0, dest_y, info.width, row_buffer.data());
        }
    }

    return decode_result::success();
}

} // namespace onyx_image
