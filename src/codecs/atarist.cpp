#include <onyx_image/codecs/atarist.hpp>
#include "byte_io.hpp"
#include "decode_helpers.hpp"

#include <array>
#include <cstring>
#include <vector>

namespace onyx_image {

namespace {

// File size constants
constexpr std::size_t NEO_FILE_SIZE = 32128;
constexpr std::size_t NEO_HEADER_SIZE = 128;
constexpr std::size_t DEGAS_STANDARD_SIZE = 32034;
constexpr std::size_t DEGAS_ELITE_SIZE = 32066;
constexpr std::uint8_t DEGAS_COMPRESSED = 0x80;

// Resolution constants
constexpr std::uint8_t ST_RES_LOW = 0;
constexpr std::uint8_t ST_RES_MEDIUM = 1;
constexpr std::uint8_t ST_RES_HIGH = 2;

// Convert ST 9-bit color (0RRR0GGG0BBB) to RGB888
void st_color_to_rgb(std::uint16_t st_color, std::uint8_t* rgb) {
    // Extract 3-bit components
    int r = (st_color >> 8) & 7;
    int g = (st_color >> 4) & 7;
    int b = st_color & 7;

    // Scale from 3-bit (0-7) to 8-bit (0-255)
    // Formula: c * 255 / 7 ≈ c * 36 + c / 2
    rgb[0] = static_cast<std::uint8_t>((r << 5) | (r << 2) | (r >> 1));
    rgb[1] = static_cast<std::uint8_t>((g << 5) | (g << 2) | (g >> 1));
    rgb[2] = static_cast<std::uint8_t>((b << 5) | (b << 2) | (b >> 1));
}

// Convert STE 12-bit color (0000rRRRgGGGbBBB - LSB bits interleaved) to RGB888
void ste_color_to_rgb(std::uint16_t ste_color, std::uint8_t* rgb) {
    // STE has 4 bits per component stored as: bit3 is LSB, bits 2-0 are MSBs
    // Red: high byte bits 3,2,1,0 where bit 3 is LSB
    // Green: low byte bits 7,6,5,4 where bit 7 is LSB (well, gb bits 7,6,5,4 maps to bit 3 being LSB)
    // Blue: low byte bits 3,2,1,0 where bit 3 is LSB
    int r_byte = (ste_color >> 8) & 0xFF;
    int gb_byte = ste_color & 0xFF;

    // RECOIL formula: rgb = (r & 7) << 17 | (r & 8) << 13 | (gb & 112) << 5 | (gb & 135) << 1 | (gb & 8) >> 3
    // Then: return rgb << 4 | rgb for 4-bit to 8-bit expansion
    int rgb_packed = ((r_byte & 7) << 17) | ((r_byte & 8) << 13) | ((gb_byte & 112) << 5) | ((gb_byte & 135) << 1) |
                     ((gb_byte & 8) >> 3);
    rgb_packed = (rgb_packed << 4) | rgb_packed;

    rgb[0] = static_cast<std::uint8_t>((rgb_packed >> 16) & 0xFF);
    rgb[1] = static_cast<std::uint8_t>((rgb_packed >> 8) & 0xFF);
    rgb[2] = static_cast<std::uint8_t>(rgb_packed & 0xFF);
}

// Check if palette data uses STE extended bits
bool is_ste_palette(const std::uint8_t* data, std::size_t offset, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        std::uint8_t r_byte = data[offset + i * 2];
        std::uint8_t gb_byte = data[offset + i * 2 + 1];
        // STE palette has bit 3 set in red byte or bits 3/7 set in gb byte
        if ((r_byte & 8) != 0 || (gb_byte & 136) != 0) {
            return true;
        }
    }
    return false;
}

// Decode interleaved bitplanes to indexed pixels
// For each 16 pixels, there are 'bitplanes' consecutive 16-bit words
void decode_st_bitplanes(const std::uint8_t* src, std::size_t src_stride, std::uint8_t* dst, int width, int height,
                         int bitplanes) {
    for (int y = 0; y < height; ++y) {
        const std::uint8_t* row = src + static_cast<std::size_t>(y) * src_stride;
        std::uint8_t* out = dst + static_cast<std::size_t>(y) * static_cast<std::size_t>(width);

        for (int x = 0; x < width; ++x) {
            // Find word group (each group is 16 pixels = bitplanes words)
            int group = x / 16;
            int bit_pos = 15 - (x % 16);

            // Base offset for this group
            std::size_t base = static_cast<std::size_t>(group) * static_cast<std::size_t>(bitplanes) * 2;

            // Build pixel value from each bitplane
            std::uint8_t pixel = 0;
            for (int plane = 0; plane < bitplanes; ++plane) {
                std::uint16_t word = read_be16(row + base + plane * 2);
                if (word & (1 << bit_pos)) {
                    pixel |= (1 << plane);
                }
            }
            out[x] = pixel;
        }
    }
}

// PackBits RLE stream decoder
class packed_bits_reader {
public:
    packed_bits_reader(const std::uint8_t* data, std::size_t size)
        : data_(data), size_(size), pos_(0), repeat_count_(0), repeat_value_(0) {}

    int read_byte() {
        if (pos_ >= size_)
            return -1;
        return data_[pos_++];
    }

    int read_rle() {
        while (repeat_count_ == 0) {
            int b = read_byte();
            if (b < 0)
                return -1;

            if (b < 128) {
                // Literal run: b+1 bytes follow
                repeat_count_ = b + 1;
                repeat_value_ = -1;  // Signal: read from stream
            } else if (b > 128) {
                // RLE run: repeat next byte (257-b) times
                repeat_count_ = 257 - b;
                repeat_value_ = read_byte();
                if (repeat_value_ < 0)
                    return -1;
            }
            // b == 128 is a no-op, continue loop
        }

        repeat_count_--;
        if (repeat_value_ >= 0) {
            return repeat_value_;
        }
        return read_byte();
    }

private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t pos_;
    int repeat_count_;
    int repeat_value_;
};

// Decompress DEGAS with per-scanline PackBits (bitplane interleaved)
// Follows RECOIL's UnpackBitplaneLines algorithm
bool unpack_degas_packbits(const std::uint8_t* src, std::size_t src_size, std::uint8_t* dst, int width, int height,
                           int bitplanes) {
    packed_bits_reader reader(src, src_size);

    int bytes_per_bitplane = ((width + 15) / 16) * 2;
    int bytes_per_line = bitplanes * bytes_per_bitplane;

    for (int y = 0; y < height; ++y) {
        // Reorder bitplane lines to bitplane words
        for (int bitplane = 0; bitplane < bitplanes; ++bitplane) {
            for (int w = bitplane * 2; w < bytes_per_line; w += bitplanes * 2) {
                for (int x = 0; x < 2; ++x) {
                    int b = reader.read_rle();
                    if (b < 0)
                        return false;
                    dst[y * bytes_per_line + w + x] = static_cast<std::uint8_t>(b);
                }
            }
        }
    }

    return true;
}

}  // namespace

// ============================================================================
// NEO Decoder
// ============================================================================

bool neo_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    // NEO files are exactly 32128 bytes
    if (data.size() != NEO_FILE_SIZE) {
        return false;
    }

    // First two bytes must be 0
    if (data[0] != 0 || data[1] != 0) {
        return false;
    }

    // Resolution must be 0, 1, or 2
    std::uint16_t res = read_be16(data.data() + 2);
    return res <= 2;
}

decode_result neo_decoder::decode(std::span<const std::uint8_t> data, surface& surf, const decode_options& options) {
    if (data.size() != NEO_FILE_SIZE) {
        return decode_result::failure(decode_error::invalid_format, "Invalid NEO file size");
    }

    // Parse header manually (big-endian)
    // Offset 0-1: flag (must be 0)
    // Offset 2-3: resolution (0=low, 1=med, 2=high)
    // Offset 4-35: palette (16 colors * 2 bytes)
    // Offset 36-127: animation data + padding
    // Offset 128+: bitmap

    std::uint16_t flag = read_be16(data.data());
    std::uint16_t resolution = read_be16(data.data() + 2);

    if (flag != 0) {
        return decode_result::failure(decode_error::invalid_format, "Invalid NEO flag");
    }

    // Determine dimensions and bitplanes based on resolution
    int width, height, bitplanes, num_colors;
    switch (resolution) {
        case ST_RES_LOW:
            width = 320;
            height = 200;
            bitplanes = 4;
            num_colors = 16;
            break;
        case ST_RES_MEDIUM:
            width = 640;
            height = 200;
            bitplanes = 2;
            num_colors = 4;
            break;
        case ST_RES_HIGH:
            width = 640;
            height = 400;
            bitplanes = 1;
            num_colors = 2;
            break;
        default:
            return decode_result::failure(decode_error::unsupported_version, "Unknown NEO resolution");
    }

    // Check dimension limits
    auto dim_result = validate_dimensions(width, height, options);
    if (!dim_result)
        return dim_result;

    // Allocate surface
    if (!surf.set_size(width, height, pixel_format::indexed8)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    // Convert ST palette to RGB
    std::array<std::uint8_t, 256 * 3> palette{};
    const std::uint8_t* pal_ptr = data.data() + 4;
    for (std::size_t i = 0; i < static_cast<std::size_t>(num_colors); ++i) {
        std::uint16_t st_color = read_be16(pal_ptr + i * 2);
        st_color_to_rgb(st_color, &palette[i * 3]);
    }
    surf.set_palette_size(num_colors);
    surf.write_palette(0, std::span<const std::uint8_t>(palette.data(), static_cast<std::size_t>(num_colors) * 3));

    // Decode bitplanes
    const std::uint8_t* bitmap = data.data() + NEO_HEADER_SIZE;
    std::size_t stride = static_cast<std::size_t>(((width + 15) / 16) * bitplanes * 2);

    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height));
    decode_st_bitplanes(bitmap, stride, pixels.data(), width, height, bitplanes);

    // Write to surface
    for (int y = 0; y < height; ++y) {
        surf.write_pixels(0, y, width, pixels.data() + y * width);
    }

    return decode_result::success();
}

// ============================================================================
// DEGAS Decoder
// ============================================================================

bool degas_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    if (data.size() < 34) {
        return false;
    }

    std::uint8_t high_byte = data[0];
    std::uint8_t resolution = data[1];

    // Uncompressed: high byte is 0, resolution is 0-2
    if (high_byte == 0 && resolution <= 2) {
        // Check for valid uncompressed sizes
        return data.size() == DEGAS_STANDARD_SIZE || data.size() == DEGAS_ELITE_SIZE || data.size() == 32128;
    }

    // Compressed: high byte is 0x80, resolution is 0-2
    if (high_byte == DEGAS_COMPRESSED && resolution <= 2) {
        return true;
    }

    return false;
}

decode_result degas_decoder::decode(std::span<const std::uint8_t> data, surface& surf, const decode_options& options) {
    if (data.size() < 34) {
        return decode_result::failure(decode_error::truncated_data, "DEGAS file too small");
    }

    // Parse header manually (big-endian)
    // Offset 0: compression flag (0 = uncompressed, 0x80 = compressed)
    // Offset 1: resolution (0=low, 1=med, 2=high)
    // Offset 2-33: palette (16 colors * 2 bytes)
    // Offset 34+: bitmap or compressed data

    bool compressed = (data[0] == DEGAS_COMPRESSED);
    int resolution = data[1];

    if (resolution > 2) {
        return decode_result::failure(decode_error::unsupported_version, "Unknown DEGAS resolution");
    }

    // Determine dimensions and bitplanes
    int width, height, bitplanes, num_colors;
    switch (resolution) {
        case ST_RES_LOW:
            width = 320;
            height = 200;
            bitplanes = 4;
            num_colors = 16;
            break;
        case ST_RES_MEDIUM:
            width = 640;
            height = 200;
            bitplanes = 2;
            num_colors = 4;
            break;
        case ST_RES_HIGH:
            width = 640;
            height = 400;
            bitplanes = 1;
            num_colors = 2;
            break;
        default:
            return decode_result::failure(decode_error::unsupported_version, "Unknown DEGAS resolution");
    }

    // Check dimension limits
    auto dim_result = validate_dimensions(width, height, options);
    if (!dim_result)
        return dim_result;

    // Allocate surface
    if (!surf.set_size(width, height, pixel_format::indexed8)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    // Convert ST palette to RGB
    std::array<std::uint8_t, 256 * 3> palette{};
    const std::uint8_t* pal_ptr = data.data() + 2;
    for (std::size_t i = 0; i < static_cast<std::size_t>(num_colors); ++i) {
        std::uint16_t st_color = read_be16(pal_ptr + i * 2);
        st_color_to_rgb(st_color, &palette[i * 3]);
    }
    surf.set_palette_size(num_colors);
    surf.write_palette(0, std::span<const std::uint8_t>(palette.data(), static_cast<std::size_t>(num_colors) * 3));

    std::size_t stride = static_cast<std::size_t>(((width + 15) / 16) * bitplanes * 2);
    std::vector<std::uint8_t> bitmap(stride * static_cast<std::size_t>(height));

    if (compressed) {
        // Decompress PackBits data
        if (!unpack_degas_packbits(data.data() + 34, data.size() - 34, bitmap.data(), width, height, bitplanes)) {
            return decode_result::failure(decode_error::unsupported_encoding, "DEGAS decompression failed");
        }
    } else {
        // Uncompressed: just copy bitmap data
        if (data.size() < 34 + bitmap.size()) {
            return decode_result::failure(decode_error::truncated_data, "DEGAS file too small for bitmap");
        }
        std::memcpy(bitmap.data(), data.data() + 34, bitmap.size());
    }

    // Decode bitplanes
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height));
    decode_st_bitplanes(bitmap.data(), stride, pixels.data(), width, height, bitplanes);

    // Write to surface
    for (int y = 0; y < height; ++y) {
        surf.write_pixels(0, y, width, pixels.data() + y * width);
    }

    return decode_result::success();
}

// ============================================================================
// Doodle Decoder
// ============================================================================

bool doodle_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    // DOO files are exactly 32000 bytes (640x400 monochrome bitmap)
    if (data.size() != 32000)
        return false;

    // Exclude files that match other formats
    // Crack Art starts with "CA"
    if (data[0] == 'C' && data[1] == 'A')
        return false;

    return true;
}

decode_result doodle_decoder::decode(std::span<const std::uint8_t> data, surface& surf, const decode_options& options) {
    if (data.size() != 32000) {
        return decode_result::failure(decode_error::invalid_format, "Invalid DOO file size");
    }

    constexpr int width = 640;
    constexpr int height = 400;

    // Check dimension limits
    auto dim_result = validate_dimensions(width, height, options);
    if (!dim_result)
        return dim_result;

    if (!surf.set_size(width, height, pixel_format::rgb888)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    // Decode monochrome bitmap (1 bit per pixel, MSB first)
    std::vector<std::uint8_t> row(static_cast<std::size_t>(width) * 3);
    for (int y = 0; y < height; ++y) {
        const std::uint8_t* src = data.data() + static_cast<std::size_t>(y) * 80;
        for (int x = 0; x < width; ++x) {
            int byte_idx = x / 8;
            int bit_idx = 7 - (x % 8);
            bool pixel = (src[byte_idx] >> bit_idx) & 1;
            std::uint8_t color = pixel ? 0x00 : 0xFF;  // 1=black, 0=white
            row[static_cast<std::size_t>(x) * 3 + 0] = color;
            row[static_cast<std::size_t>(x) * 3 + 1] = color;
            row[static_cast<std::size_t>(x) * 3 + 2] = color;
        }
        surf.write_pixels(0, y, width * 3, row.data());
    }

    return decode_result::success();
}

// ============================================================================
// Crack Art Decoder
// ============================================================================

namespace {

// Crack Art RLE stream decoder
class CaStreamReader {
public:
    CaStreamReader(const std::uint8_t* data, std::size_t size, std::size_t offset)
        : data_(data), size_(size), pos_(offset), repeat_count_(0), repeat_value_(0), escape_(0), default_value_(0) {}

    bool init() {
        if (pos_ + 4 > size_)
            return false;
        escape_ = data_[pos_];
        default_value_ = data_[pos_ + 1];
        unpack_step_ = read_be16(data_ + pos_ + 2);
        if (unpack_step_ >= 32000)
            return false;
        pos_ += 4;
        if (unpack_step_ == 0) {
            repeat_count_ = 32000;
            repeat_value_ = default_value_;
            unpack_step_ = 1;
        }
        return true;
    }

    int read_byte() {
        if (pos_ >= size_)
            return -1;
        return data_[pos_++];
    }

    bool read_command() {
        int b = read_byte();
        if (b < 0)
            return false;

        if (b != escape_) {
            repeat_count_ = 1;
            repeat_value_ = b;
            return true;
        }

        int c = read_byte();
        if (c < 0)
            return false;

        if (c == escape_) {
            repeat_count_ = 1;
            repeat_value_ = c;
            return true;
        }

        b = read_byte();
        if (b < 0)
            return false;

        switch (c) {
            case 0:
                repeat_count_ = b + 1;
                repeat_value_ = read_byte();
                break;
            case 1: {
                int c2 = read_byte();
                if (c2 < 0)
                    return false;
                repeat_count_ = (b << 8) + c2 + 1;
                repeat_value_ = read_byte();
                break;
            }
            case 2:
                if (b == 0) {
                    repeat_count_ = 32000;  // end decompression
                } else {
                    int c2 = read_byte();
                    if (c2 < 0)
                        return false;
                    repeat_count_ = (b << 8) + c2 + 1;
                }
                repeat_value_ = default_value_;
                break;
            default:
                repeat_count_ = c + 1;
                repeat_value_ = b;
                break;
        }
        return true;
    }

    int read_rle() {
        while (repeat_count_ == 0) {
            if (!read_command())
                return -1;
        }
        repeat_count_--;
        if (repeat_value_ >= 0) {
            return repeat_value_;
        }
        return read_byte();
    }

    bool unpack_columns(std::uint8_t* dst, std::size_t dst_size) {
        for (std::size_t col = 0; col < static_cast<std::size_t>(unpack_step_); ++col) {
            for (std::size_t offset = col; offset < dst_size; offset += static_cast<std::size_t>(unpack_step_)) {
                int b = read_rle();
                if (b < 0)
                    return false;
                dst[offset] = static_cast<std::uint8_t>(b);
            }
        }
        return true;
    }

private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t pos_;
    int repeat_count_;
    int repeat_value_;
    int escape_;
    int default_value_;
    int unpack_step_ = 1;
};

}  // namespace

bool crack_art_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    if (data.size() < 8)
        return false;
    // Check for "CA" signature
    if (data[0] != 'C' || data[1] != 'A')
        return false;
    // data[2] is compression (0 or 1)
    if (data[2] > 1)
        return false;
    // data[3] is resolution (0, 1, or 2)
    if (data[3] > 2)
        return false;
    return true;
}

decode_result crack_art_decoder::decode(std::span<const std::uint8_t> data, surface& surf,
                                        const decode_options& options) {
    if (data.size() < 8 || data[0] != 'C' || data[1] != 'A') {
        return decode_result::failure(decode_error::invalid_format, "Invalid Crack Art signature");
    }

    int compression = data[2];
    int resolution = data[3];

    if (compression > 1 || resolution > 2) {
        return decode_result::failure(decode_error::unsupported_version,
                                      "Unsupported Crack Art compression/resolution");
    }

    // Content offset depends on resolution
    std::size_t content_offset;
    switch (resolution) {
        case ST_RES_LOW:
            content_offset = 4 + 32;
            break;  // 16 colors * 2 bytes
        case ST_RES_MEDIUM:
            content_offset = 4 + 8;
            break;  // 4 colors * 2 bytes
        case ST_RES_HIGH:
            content_offset = 4;
            break;  // 2 colors (black/white)
        default:
            return decode_result::failure(decode_error::unsupported_version, "Unknown resolution");
    }

    int width, height, bitplanes, num_colors;
    switch (resolution) {
        case ST_RES_LOW:
            width = 320;
            height = 200;
            bitplanes = 4;
            num_colors = 16;
            break;
        case ST_RES_MEDIUM:
            width = 640;
            height = 200;
            bitplanes = 2;
            num_colors = 4;
            break;
        case ST_RES_HIGH:
            width = 640;
            height = 400;
            bitplanes = 1;
            num_colors = 2;
            break;
        default:
            return decode_result::failure(decode_error::unsupported_version, "Unknown resolution");
    }

    // Check dimension limits
    auto dim_result = validate_dimensions(width, height, options);
    if (!dim_result)
        return dim_result;

    // Unpack bitmap
    std::vector<std::uint8_t> bitmap(32000);
    if (compression == 0) {
        // Uncompressed
        if (content_offset + 32000 != data.size()) {
            return decode_result::failure(decode_error::invalid_format, "Invalid uncompressed CA size");
        }
        std::memcpy(bitmap.data(), data.data() + content_offset, 32000);
    } else {
        // Compressed
        CaStreamReader reader(data.data(), data.size(), content_offset);
        if (!reader.init() || !reader.unpack_columns(bitmap.data(), 32000)) {
            return decode_result::failure(decode_error::unsupported_encoding, "CA decompression failed");
        }
    }

    if (!surf.set_size(width, height, pixel_format::indexed8)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    // Convert palette
    std::array<std::uint8_t, 256 * 3> palette{};
    const std::uint8_t* pal_ptr = data.data() + 4;
    for (std::size_t i = 0; i < static_cast<std::size_t>(num_colors); ++i) {
        std::uint16_t st_color = read_be16(pal_ptr + i * 2);
        st_color_to_rgb(st_color, &palette[i * 3]);
    }
    surf.set_palette_size(num_colors);
    surf.write_palette(0, std::span<const std::uint8_t>(palette.data(), static_cast<std::size_t>(num_colors) * 3));

    // Decode bitplanes
    std::size_t stride = static_cast<std::size_t>(((width + 15) / 16) * bitplanes * 2);
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height));
    decode_st_bitplanes(bitmap.data(), stride, pixels.data(), width, height, bitplanes);

    for (int y = 0; y < height; ++y) {
        surf.write_pixels(0, y, width, pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width));
    }

    return decode_result::success();
}

// ============================================================================
// Tiny Stuff Decoder
// ============================================================================

namespace {

class tny_stream_reader {
public:
    tny_stream_reader(const std::uint8_t* data, std::size_t size)
        : data_(data),
          size_(size),
          ctrl_pos_(0),
          ctrl_end_(0),
          val_pos_(0),
          val_end_(0),
          repeat_count_(0),
          repeat_value_(0) {}

    void init(std::size_t ctrl_offset, std::size_t ctrl_len, std::size_t val_offset, std::size_t val_len) {
        ctrl_pos_ = ctrl_offset;
        ctrl_end_ = ctrl_offset + ctrl_len;
        val_pos_ = val_offset;
        val_end_ = val_offset + val_len;
    }

    int read_ctrl_byte() {
        if (ctrl_pos_ >= ctrl_end_ || ctrl_pos_ >= size_)
            return -1;
        return data_[ctrl_pos_++];
    }

    int read_value() {
        if (val_pos_ + 1 >= val_end_ || val_pos_ + 1 >= size_)
            return -1;
        int value = (data_[val_pos_] << 8) | data_[val_pos_ + 1];
        val_pos_ += 2;
        return value;
    }

    bool read_command() {
        int b = read_ctrl_byte();
        if (b < 0)
            return false;

        if (b < 128) {
            if (b == 0 || b == 1) {
                // Extended count follows
                if (ctrl_pos_ + 1 >= ctrl_end_)
                    return false;
                repeat_count_ = (data_[ctrl_pos_] << 8) | data_[ctrl_pos_ + 1];
                ctrl_pos_ += 2;
            } else {
                repeat_count_ = b;
            }
            repeat_value_ = (b == 1) ? -1 : read_value();
        } else {
            repeat_count_ = 256 - b;
            repeat_value_ = -1;
        }
        return true;
    }

    int read_rle() {
        while (repeat_count_ == 0) {
            if (!read_command())
                return -1;
        }
        repeat_count_--;
        if (repeat_value_ >= 0) {
            return repeat_value_;
        }
        return read_value();
    }

private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t ctrl_pos_;
    std::size_t ctrl_end_;
    std::size_t val_pos_;
    std::size_t val_end_;
    int repeat_count_;
    int repeat_value_;
};

}  // namespace

bool tiny_stuff_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    if (data.size() < 42)
        return false;
    int mode = data[0];
    // Mode 0-2: standard, 3-5: with animation header
    if (mode > 5)
        return false;

    std::size_t content_offset = (mode > 2) ? 4 : 0;

    // Verify control and value lengths make sense
    std::size_t control_length = (data[content_offset + 33] << 8) | data[content_offset + 34];
    std::size_t value_length = static_cast<std::size_t>((data[content_offset + 35] << 8) | data[content_offset + 36])
                               << 1;

    // Check that the file size matches the expected structure
    std::size_t expected_size = content_offset + 37 + control_length + value_length;
    return data.size() >= expected_size && data.size() <= expected_size + 16;  // Allow small padding
}

decode_result tiny_stuff_decoder::decode(std::span<const std::uint8_t> data, surface& surf,
                                         const decode_options& options) {
    if (data.size() < 42) {
        return decode_result::failure(decode_error::truncated_data, "TNY file too small");
    }

    int mode = data[0];
    std::size_t content_offset;
    if (mode > 2) {
        if (mode > 5) {
            return decode_result::failure(decode_error::unsupported_version, "Invalid TNY mode");
        }
        mode -= 3;
        content_offset = 4;  // Skip animation header
    } else {
        content_offset = 0;
    }

    // Read control and value lengths
    std::size_t control_length = (data[content_offset + 33] << 8) | data[content_offset + 34];
    std::size_t value_length = static_cast<std::size_t>((data[content_offset + 35] << 8) | data[content_offset + 36])
                               << 1;

    if (content_offset + 37 + control_length + value_length > data.size()) {
        return decode_result::failure(decode_error::truncated_data, "TNY file truncated");
    }

    tny_stream_reader reader(data.data(), data.size());
    reader.init(content_offset + 37, control_length, content_offset + 37 + control_length, value_length);

    // Decompress bitmap
    std::vector<std::uint8_t> bitmap(32000);
    for (int bitplane = 0; bitplane < 8; bitplane += 2) {
        for (int x = bitplane; x < 160; x += 8) {
            for (int y = 0; y < 200; ++y) {
                int offset = y * 160 + x;
                int b = reader.read_rle();
                if (b < 0) {
                    return decode_result::failure(decode_error::unsupported_encoding, "TNY decompression failed");
                }
                bitmap[static_cast<std::size_t>(offset)] = static_cast<std::uint8_t>(b >> 8);
                bitmap[static_cast<std::size_t>(offset + 1)] = static_cast<std::uint8_t>(b & 0xFF);
            }
        }
    }

    int width, height, bitplanes, num_colors;
    switch (mode) {
        case ST_RES_LOW:
            width = 320;
            height = 200;
            bitplanes = 4;
            num_colors = 16;
            break;
        case ST_RES_MEDIUM:
            width = 640;
            height = 200;
            bitplanes = 2;
            num_colors = 4;
            break;
        case ST_RES_HIGH:
            width = 640;
            height = 400;
            bitplanes = 1;
            num_colors = 2;
            break;
        default:
            return decode_result::failure(decode_error::unsupported_version, "Unknown resolution");
    }

    // Check dimension limits
    auto dim_result = validate_dimensions(width, height, options);
    if (!dim_result)
        return dim_result;

    if (!surf.set_size(width, height, pixel_format::indexed8)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    // Convert palette (at content_offset + 1)
    std::array<std::uint8_t, 256 * 3> palette{};
    const std::uint8_t* pal_ptr = data.data() + content_offset + 1;
    for (std::size_t i = 0; i < static_cast<std::size_t>(num_colors); ++i) {
        std::uint16_t st_color = read_be16(pal_ptr + i * 2);
        st_color_to_rgb(st_color, &palette[i * 3]);
    }
    surf.set_palette_size(num_colors);
    surf.write_palette(0, std::span<const std::uint8_t>(palette.data(), static_cast<std::size_t>(num_colors) * 3));

    // Decode bitplanes
    std::size_t stride = 160;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height));
    decode_st_bitplanes(bitmap.data(), stride, pixels.data(), width, height, bitplanes);

    for (int y = 0; y < height; ++y) {
        surf.write_pixels(0, y, width, pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width));
    }

    return decode_result::success();
}

// ============================================================================
// Spectrum 512 Decoder
// ============================================================================

namespace {

// Get pixel value from Spectrum 512 bitmap using RECOIL's exact algorithm
// The bitmap uses a special byte interleaving scheme:
// - pixelsOffset is a linear index (y * 320 + x)
// - byteOffset = (pixelsOffset >> 3 & ~1) * 4 + (pixelsOffset >> 3 & 1)
// - bit = ~pixelsOffset & 7
inline int get_spectrum512_pixel(const std::uint8_t* bitmap, std::size_t bitmapOffset, int pixelsOffset) {
    // Calculate byte offset using RECOIL's interleaving formula
    int idx = pixelsOffset >> 3;
    std::size_t byteOffset = static_cast<std::size_t>((idx & ~1) * 4 + (idx & 1));
    std::size_t base = bitmapOffset + byteOffset;
    int bit = ~pixelsOffset & 7;

    // Read 4 bitplanes, each 2 bytes apart (word-interleaved)
    int pixel = 0;
    for (int plane = 3; plane >= 0; --plane) {
        pixel = (pixel << 1) | ((bitmap[base + static_cast<std::size_t>(plane) * 2] >> bit) & 1);
    }
    return pixel;
}

class SpcStreamReader {
public:
    SpcStreamReader(const std::uint8_t* data, std::size_t size, std::size_t offset)
        : data_(data), size_(size), pos_(offset), repeat_count_(0), repeat_value_(0) {}

    int read_byte() {
        if (pos_ >= size_)
            return -1;
        return data_[pos_++];
    }

    bool read_command() {
        int b = read_byte();
        if (b < 0)
            return false;
        if (b < 128) {
            repeat_count_ = b + 1;
            repeat_value_ = -1;
        } else {
            repeat_count_ = 258 - b;
            repeat_value_ = read_byte();
        }
        return true;
    }

    int read_rle() {
        while (repeat_count_ == 0) {
            if (!read_command())
                return -1;
        }
        repeat_count_--;
        if (repeat_value_ >= 0)
            return repeat_value_;
        return read_byte();
    }

    bool unpack_words(std::uint8_t* dst, std::size_t offset, std::size_t step, std::size_t end) {
        for (std::size_t i = offset; i < end; i += step) {
            int hi = read_rle();
            int lo = read_rle();
            if (hi < 0 || lo < 0)
                return false;
            dst[i] = static_cast<std::uint8_t>(hi);
            dst[i + 1] = static_cast<std::uint8_t>(lo);
        }
        return true;
    }

    std::size_t position() const { return pos_; }

private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t pos_;
    int repeat_count_;
    int repeat_value_;
};

}  // namespace

bool spectrum512_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    // SPU: exactly 51104 bytes
    if (data.size() == 51104)
        return true;
    // SPC: starts with "SP"
    if (data.size() >= 12 && data[0] == 'S' && data[1] == 'P')
        return true;
    return false;
}

decode_result spectrum512_decoder::decode(std::span<const std::uint8_t> data, surface& surf,
                                          const decode_options& options) {
    constexpr int width = 320;
    constexpr int height = 199;

    // Check dimension limits
    auto dim_result = validate_dimensions(width, height, options);
    if (!dim_result)
        return dim_result;

    std::vector<std::uint8_t> unpacked(51104);
    bool is_spc = (data.size() >= 12 && data[0] == 'S' && data[1] == 'P');

    if (is_spc) {
        // SPC compressed format
        if (data.size() < 12) {
            return decode_result::failure(decode_error::truncated_data, "SPC file too small");
        }

        // Unpack bitmap - matches RECOIL's UnpackSpc algorithm
        // Writes to offsets 160+bitplane, 168+bitplane, 176+bitplane, ... (step 8)
        // bitplane goes 0, 2, 4, 6 (not 0, 1, 2, 3)
        SpcStreamReader reader(data.data(), data.size(), 12);
        for (int bitplane = 0; bitplane < 8; bitplane += 2) {
            if (!reader.unpack_words(unpacked.data(), 160 + static_cast<std::size_t>(bitplane), 8, 32000)) {
                return decode_result::failure(decode_error::unsupported_encoding, "SPC bitmap decompression failed");
            }
        }

        // Unpack palettes
        std::size_t palette_offset =
            12 + static_cast<std::size_t>((data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7]);
        if (palette_offset < 12 || palette_offset >= data.size()) {
            return decode_result::failure(decode_error::invalid_format, "Invalid SPC palette offset");
        }

        std::size_t pos = palette_offset;
        for (std::size_t unpacked_offset = 32000; unpacked_offset < 51104;) {
            if (pos + 1 >= data.size()) {
                return decode_result::failure(decode_error::truncated_data, "SPC palette truncated");
            }
            int got = ((data[pos] & 0x7F) << 8) | data[pos + 1];
            pos += 2;
            for (int i = 0; i < 16; ++i) {
                if ((got >> i & 1) == 0) {
                    unpacked[unpacked_offset] = 0;
                    unpacked[unpacked_offset + 1] = 0;
                } else {
                    if (pos + 1 >= data.size()) {
                        return decode_result::failure(decode_error::truncated_data, "SPC palette truncated");
                    }
                    unpacked[unpacked_offset] = data[pos];
                    unpacked[unpacked_offset + 1] = data[pos + 1];
                    pos += 2;
                }
                unpacked_offset += 2;
            }
        }
    } else if (data.size() == 51104) {
        // SPU uncompressed format
        std::memcpy(unpacked.data(), data.data(), 51104);
    } else {
        return decode_result::failure(decode_error::invalid_format, "Invalid Spectrum 512 format");
    }

    if (!surf.set_size(width, height, pixel_format::rgb888)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    // Decode with per-scanline palette using RECOIL's exact algorithm
    // bitmapOffset = 160 (first 160 bytes are skipped/unused)
    // pixelsOffset = y * 320 + x (linear pixel index)
    std::vector<std::uint8_t> row(static_cast<std::size_t>(width) * 3);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int pixelsOffset = y * 320 + x;
            int c = get_spectrum512_pixel(unpacked.data(), 160, pixelsOffset);

            // Spectrum 512 palette selection based on x position
            // http://www.atari-forum.com/wiki/index.php?title=ST_Picture_Formats
            int x1 = c * 10 + 1 - (c & 1) * 6;
            if (x >= x1 + 160) {
                c += 32;
            } else if (x >= x1) {
                c += 16;
            }

            std::size_t color_offset = 32000 + static_cast<std::size_t>(y) * 96 + static_cast<std::size_t>(c) * 2;
            std::uint16_t st_color = read_be16(unpacked.data() + color_offset);
            st_color_to_rgb(st_color, row.data() + static_cast<std::size_t>(x) * 3);
        }
        surf.write_pixels(0, y, width * 3, row.data());
    }

    return decode_result::success();
}

// ============================================================================
// Photochrome Decoder
// ============================================================================

namespace {

// Get pixel value from separated bitplanes (Photochrome format)
// Uses RECOIL's exact algorithm: builds pixel from MSB (plane 3) to LSB (plane 0)
int get_st_low_separate_bitplanes(const std::uint8_t* data, std::size_t offset, std::size_t plane_stride, int x) {
    std::size_t byte_idx = static_cast<std::size_t>(x >> 3);
    int bit = ~x & 7;
    int pixel = 0;
    // Loop from plane 3 down to 0 (MSB to LSB)
    for (int plane = 3; plane >= 0; --plane) {
        std::uint8_t byte = data[offset + byte_idx + static_cast<std::size_t>(plane) * plane_stride];
        pixel = (pixel << 1) | ((byte >> bit) & 1);
    }
    return pixel;
}

class pcs_stream_reader {
public:
    pcs_stream_reader(const std::uint8_t* data, std::size_t size, std::size_t offset)
        : data_(data),
          size_(size),
          pos_(offset),
          repeat_count_(0),
          repeat_value_(0),
          command_count_(0),
          is_palette_(false) {}

    int read_byte() {
        if (pos_ >= size_)
            return -1;
        return data_[pos_++];
    }

    int read_value() {
        if (!is_palette_) {
            return read_byte();
        }
        // Word for palette
        if (pos_ + 1 >= size_)
            return -1;
        int value = (data_[pos_] << 8) | data_[pos_ + 1];
        pos_ += 2;
        return value;
    }

    bool read_command() {
        if (command_count_ <= 0)
            return false;
        command_count_--;

        int b = read_byte();
        if (b < 0)
            return false;

        if (b < 128) {
            if (b == 0 || b == 1) {
                if (pos_ + 1 >= size_)
                    return false;
                repeat_count_ = (data_[pos_] << 8) | data_[pos_ + 1];
                pos_ += 2;
            } else {
                repeat_count_ = b;
            }
            repeat_value_ = (b == 1) ? -1 : read_value();
        } else {
            repeat_count_ = 256 - b;
            repeat_value_ = -1;
        }
        return true;
    }

    int read_rle() {
        while (repeat_count_ == 0) {
            if (!read_command())
                return -1;
        }
        repeat_count_--;
        if (repeat_value_ >= 0)
            return repeat_value_;
        return read_value();
    }

    bool start_block() {
        if (pos_ + 1 >= size_)
            return false;
        command_count_ = (data_[pos_] << 8) | data_[pos_ + 1];
        pos_ += 2;
        return true;
    }

    void end_block() {
        while (repeat_count_ > 0 || command_count_ > 0) {
            if (read_rle() < 0)
                break;
        }
    }

    static constexpr std::size_t UNPACKED_LENGTH = 32000 + (199 * 3 + 1) * 32;

    bool unpack(std::uint8_t* dst, std::size_t offset, std::size_t step, std::size_t end) {
        for (std::size_t i = offset; i < end; i += step) {
            int b = read_rle();
            if (b < 0)
                return false;
            dst[i] = static_cast<std::uint8_t>(b);
        }
        return true;
    }

    bool unpack_pcs(std::uint8_t* unpacked) {
        // Bitmap - single block, bytes
        is_palette_ = false;
        if (!start_block())
            return false;
        if (!unpack(unpacked, 0, 1, 32000))
            return false;
        end_block();

        // Palettes - single block, words
        is_palette_ = true;
        if (!start_block())
            return false;
        for (std::size_t offset = 32000; offset < UNPACKED_LENGTH; offset += 2) {
            int b = read_rle();
            if (b < 0)
                return false;
            unpacked[offset] = static_cast<std::uint8_t>(b >> 8);
            unpacked[offset + 1] = static_cast<std::uint8_t>(b & 0xFF);
        }
        end_block();

        return true;
    }

private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t pos_;
    int repeat_count_;
    int repeat_value_;
    int command_count_;
    bool is_palette_;
};

}  // namespace

bool photochrome_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    if (data.size() < 18)
        return false;
    // Check header: 01 40 00 C8
    return data[0] == 0x01 && data[1] == 0x40 && data[2] == 0x00 && data[3] == 0xC8;
}

decode_result photochrome_decoder::decode(std::span<const std::uint8_t> data, surface& surf,
                                          const decode_options& options) {
    if (data.size() < 18) {
        return decode_result::failure(decode_error::truncated_data, "PCS file too small");
    }

    if (data[0] != 0x01 || data[1] != 0x40 || data[2] != 0x00 || data[3] != 0xC8) {
        return decode_result::failure(decode_error::invalid_format, "Invalid PCS header");
    }

    constexpr int width = 320;
    constexpr int height = 199;

    // Check dimension limits
    auto dim_result = validate_dimensions(width, height, options);
    if (!dim_result)
        return dim_result;

    std::vector<std::uint8_t> unpacked(pcs_stream_reader::UNPACKED_LENGTH);
    pcs_stream_reader reader(data.data(), data.size(), 6);
    if (!reader.unpack_pcs(unpacked.data())) {
        return decode_result::failure(decode_error::unsupported_encoding, "PCS decompression failed");
    }

    if (!surf.set_size(width, height, pixel_format::rgb888)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    // Check if palette uses STE extended bits (4-bit per channel vs 3-bit)
    // PCS palette: 199 scanlines * 48 palette entries = 9552 entries (check them all)
    constexpr int palette_entries = (pcs_stream_reader::UNPACKED_LENGTH - 32000) / 2;
    bool use_ste = is_ste_palette(unpacked.data(), 32000, palette_entries);

    // Decode with per-scanline palette sections
    std::vector<std::uint8_t> row(static_cast<std::size_t>(width) * 3);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int c = get_st_low_separate_bitplanes(unpacked.data(), 40 + static_cast<std::size_t>(y) * 40, 8000, x) << 1;

            // Photochrome palette selection based on x position
            // http://www.atari-forum.com/wiki/index.php?title=ST_Picture_Formats
            if (x >= c * 2) {
                if (c < 28) {
                    if (x >= c * 2 + 76) {
                        if (x >= 176 + c * 5 - (c & 2) * 3) {
                            c += 32;
                        }
                        c += 32;
                    }
                } else if (x >= c * 2 + 92) {
                    c += 32;
                }
                c += 32;
            }

            std::size_t color_offset = 32000 + static_cast<std::size_t>(y) * 96 + static_cast<std::size_t>(c);
            std::uint16_t st_color = read_be16(unpacked.data() + color_offset);
            if (use_ste) {
                ste_color_to_rgb(st_color, row.data() + static_cast<std::size_t>(x) * 3);
            } else {
                st_color_to_rgb(st_color, row.data() + static_cast<std::size_t>(x) * 3);
            }
        }
        surf.write_pixels(0, y, width * 3, row.data());
    }

    return decode_result::success();
}

}  // namespace onyx_image
