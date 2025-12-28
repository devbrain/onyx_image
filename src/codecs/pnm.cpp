#include <onyx_image/codecs/pnm.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstring>
#include <vector>

namespace onyx_image {

namespace {

// PNM format types
constexpr int PNM_TYPE_PBM_ASCII  = 1;  // P1 - ASCII bitmap
constexpr int PNM_TYPE_PGM_ASCII  = 2;  // P2 - ASCII graymap
constexpr int PNM_TYPE_PPM_ASCII  = 3;  // P3 - ASCII pixmap
constexpr int PNM_TYPE_PBM_BINARY = 4;  // P4 - Binary bitmap
constexpr int PNM_TYPE_PGM_BINARY = 5;  // P5 - Binary graymap
constexpr int PNM_TYPE_PPM_BINARY = 6;  // P6 - Binary pixmap

struct pnm_info {
    int type = 0;      // 1-7 (P1-P7)
    int width = 0;
    int height = 0;
    int maxval = 1;    // Default 1 for PBM
    std::size_t data_offset = 0;
};

class pnm_parser {
public:
    explicit pnm_parser(std::span<const std::uint8_t> data)
        : data_(data), pos_(0) {}

    bool parse_header(pnm_info& info) {
        // Check magic number
        if (data_.size() < 3) return false;
        if (data_[0] != 'P') return false;
        if (data_[1] < '1' || data_[1] > '7') return false;

        info.type = data_[1] - '0';
        pos_ = 2;

        // Parse width
        if (!skip_whitespace_and_comments()) return false;
        if (!parse_int(info.width)) return false;
        if (info.width <= 0) return false;

        // Parse height
        if (!skip_whitespace_and_comments()) return false;
        if (!parse_int(info.height)) return false;
        if (info.height <= 0) return false;

        // Parse maxval (not present for PBM: P1, P4)
        if (info.type != PNM_TYPE_PBM_ASCII && info.type != PNM_TYPE_PBM_BINARY) {
            if (!skip_whitespace_and_comments()) return false;
            if (!parse_int(info.maxval)) return false;
            if (info.maxval <= 0 || info.maxval > 65535) return false;
        }

        // Handle whitespace/comments before pixel data differently for ASCII vs binary
        bool is_binary = (info.type == PNM_TYPE_PBM_BINARY ||
                          info.type == PNM_TYPE_PGM_BINARY ||
                          info.type == PNM_TYPE_PPM_BINARY);

        if (is_binary) {
            // For binary formats (P4/P5/P6): skip all whitespace but NOT comments
            // Binary data could start with '#' byte which should not be treated as comment
            // We require at least one whitespace separator per PNM spec
            if (pos_ >= data_.size() || !std::isspace(data_[pos_])) {
                return false;  // Missing required whitespace separator
            }
            // Skip all whitespace (but not comments) - valid files may have extra whitespace
            while (pos_ < data_.size() && std::isspace(data_[pos_])) {
                pos_++;
            }
        } else {
            // For ASCII formats (P1/P2/P3): skip all whitespace and comments
            // ASCII data starts with digits, so this is safe
            if (!skip_whitespace_and_comments()) {
                return false;
            }
        }

        info.data_offset = pos_;
        return true;
    }

private:
    bool skip_whitespace_and_comments() {
        while (pos_ < data_.size()) {
            if (data_[pos_] == '#') {
                // Skip comment until end of line
                while (pos_ < data_.size() && data_[pos_] != '\n') {
                    pos_++;
                }
                if (pos_ < data_.size()) pos_++;  // Skip newline
            } else if (std::isspace(data_[pos_])) {
                pos_++;
            } else {
                return true;
            }
        }
        return false;
    }

    bool parse_int(int& value) {
        if (pos_ >= data_.size()) return false;

        const char* start = reinterpret_cast<const char*>(data_.data() + pos_);
        const char* end = reinterpret_cast<const char*>(data_.data() + data_.size());

        auto result = std::from_chars(start, end, value);
        if (result.ec != std::errc{}) return false;

        pos_ += static_cast<std::size_t>(result.ptr - start);
        return true;
    }

    std::span<const std::uint8_t> data_;
    std::size_t pos_;
};

// Decode ASCII PBM (P1)
bool decode_pbm_ascii(std::span<const std::uint8_t> data, std::size_t offset,
                      std::size_t width, std::size_t height,
                      std::vector<std::uint8_t>& row_buffer, surface& surf) {
    std::size_t pos = offset;
    const std::size_t size = data.size();

    for (std::size_t y = 0; y < height; y++) {
        for (std::size_t x = 0; x < width; x++) {
            // Skip whitespace
            while (pos < size && std::isspace(data[pos])) pos++;
            if (pos >= size) return false;

            // Read single digit (0 or 1)
            // In PBM: 1 = black, 0 = white
            std::uint8_t val = (data[pos] == '0') ? 255 : 0;
            pos++;

            row_buffer[x * 3 + 0] = val;
            row_buffer[x * 3 + 1] = val;
            row_buffer[x * 3 + 2] = val;
        }
        surf.write_pixels(0, static_cast<int>(y), static_cast<int>(row_buffer.size()), row_buffer.data());
    }
    return true;
}

// Decode binary PBM (P4)
bool decode_pbm_binary(std::span<const std::uint8_t> data, std::size_t offset,
                       std::size_t width, std::size_t height,
                       std::vector<std::uint8_t>& row_buffer, surface& surf) {
    const std::size_t row_bytes = (width + 7) / 8;
    std::size_t pos = offset;

    for (std::size_t y = 0; y < height; y++) {
        if (pos + row_bytes > data.size()) return false;

        for (std::size_t x = 0; x < width; x++) {
            std::size_t byte_idx = x / 8;
            int bit_idx = 7 - static_cast<int>(x % 8);
            // In PBM: 1 = black, 0 = white
            std::uint8_t val = ((data[pos + byte_idx] >> bit_idx) & 1) ? 0 : 255;

            row_buffer[x * 3 + 0] = val;
            row_buffer[x * 3 + 1] = val;
            row_buffer[x * 3 + 2] = val;
        }
        surf.write_pixels(0, static_cast<int>(y), static_cast<int>(row_buffer.size()), row_buffer.data());
        pos += row_bytes;
    }
    return true;
}

// Decode ASCII PGM (P2)
bool decode_pgm_ascii(std::span<const std::uint8_t> data, std::size_t offset,
                      std::size_t width, std::size_t height, int maxval,
                      std::vector<std::uint8_t>& row_buffer, surface& surf) {
    const char* ptr = reinterpret_cast<const char*>(data.data()) + offset;
    const char* end = reinterpret_cast<const char*>(data.data()) + data.size();

    for (std::size_t y = 0; y < height; y++) {
        for (std::size_t x = 0; x < width; x++) {
            // Skip whitespace
            while (ptr < end && std::isspace(*ptr)) ptr++;
            if (ptr >= end) return false;

            int val = 0;
            auto result = std::from_chars(ptr, end, val);
            if (result.ec != std::errc{}) return false;
            ptr = result.ptr;

            // Scale to 0-255
            std::uint8_t pixel = static_cast<std::uint8_t>(val * 255 / maxval);
            row_buffer[x * 3 + 0] = pixel;
            row_buffer[x * 3 + 1] = pixel;
            row_buffer[x * 3 + 2] = pixel;
        }
        surf.write_pixels(0, static_cast<int>(y), static_cast<int>(row_buffer.size()), row_buffer.data());
    }
    return true;
}

// Decode binary PGM (P5)
bool decode_pgm_binary(std::span<const std::uint8_t> data, std::size_t offset,
                       std::size_t width, std::size_t height, int maxval,
                       std::vector<std::uint8_t>& row_buffer, surface& surf) {
    const bool is_16bit = maxval > 255;
    const std::size_t bytes_per_pixel = is_16bit ? 2 : 1;
    const std::size_t row_bytes = width * bytes_per_pixel;
    std::size_t pos = offset;

    for (std::size_t y = 0; y < height; y++) {
        if (pos + row_bytes > data.size()) return false;

        for (std::size_t x = 0; x < width; x++) {
            int val;
            if (is_16bit) {
                // Big-endian 16-bit
                val = (data[pos + x * 2] << 8) | data[pos + x * 2 + 1];
            } else {
                val = data[pos + x];
            }

            // Scale to 0-255
            std::uint8_t pixel = static_cast<std::uint8_t>(val * 255 / maxval);
            row_buffer[x * 3 + 0] = pixel;
            row_buffer[x * 3 + 1] = pixel;
            row_buffer[x * 3 + 2] = pixel;
        }
        surf.write_pixels(0, static_cast<int>(y), static_cast<int>(row_buffer.size()), row_buffer.data());
        pos += row_bytes;
    }
    return true;
}

// Decode ASCII PPM (P3)
bool decode_ppm_ascii(std::span<const std::uint8_t> data, std::size_t offset,
                      std::size_t width, std::size_t height, int maxval,
                      std::vector<std::uint8_t>& row_buffer, surface& surf) {
    const char* ptr = reinterpret_cast<const char*>(data.data()) + offset;
    const char* end = reinterpret_cast<const char*>(data.data()) + data.size();

    for (std::size_t y = 0; y < height; y++) {
        for (std::size_t x = 0; x < width; x++) {
            for (std::size_t c = 0; c < 3; c++) {
                // Skip whitespace
                while (ptr < end && std::isspace(*ptr)) ptr++;
                if (ptr >= end) return false;

                int val = 0;
                auto result = std::from_chars(ptr, end, val);
                if (result.ec != std::errc{}) return false;
                ptr = result.ptr;

                // Scale to 0-255
                row_buffer[x * 3 + c] = static_cast<std::uint8_t>(val * 255 / maxval);
            }
        }
        surf.write_pixels(0, static_cast<int>(y), static_cast<int>(row_buffer.size()), row_buffer.data());
    }
    return true;
}

// Decode binary PPM (P6)
bool decode_ppm_binary(std::span<const std::uint8_t> data, std::size_t offset,
                       std::size_t width, std::size_t height, int maxval,
                       std::vector<std::uint8_t>& row_buffer, surface& surf) {
    const bool is_16bit = maxval > 255;
    const std::size_t bytes_per_pixel = is_16bit ? 6 : 3;
    const std::size_t row_bytes = width * bytes_per_pixel;
    std::size_t pos = offset;

    for (std::size_t y = 0; y < height; y++) {
        if (pos + row_bytes > data.size()) return false;

        for (std::size_t x = 0; x < width; x++) {
            for (std::size_t c = 0; c < 3; c++) {
                int val;
                if (is_16bit) {
                    // Big-endian 16-bit
                    std::size_t idx = pos + x * 6 + c * 2;
                    val = (data[idx] << 8) | data[idx + 1];
                } else {
                    val = data[pos + x * 3 + c];
                }

                // Scale to 0-255
                row_buffer[x * 3 + c] = static_cast<std::uint8_t>(val * 255 / maxval);
            }
        }
        surf.write_pixels(0, static_cast<int>(y), static_cast<int>(row_buffer.size()), row_buffer.data());
        pos += row_bytes;
    }
    return true;
}

} // namespace

bool pnm_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    if (data.size() < 3) return false;
    if (data[0] != 'P') return false;
    if (data[1] < '1' || data[1] > '6') return false;
    // Third character must be whitespace
    return std::isspace(data[2]) != 0;
}

decode_result pnm_decoder::decode(std::span<const std::uint8_t> data,
                                   surface& surf,
                                   const decode_options& options) {
    if (!sniff(data)) {
        return decode_result::failure(decode_error::invalid_format, "Not a valid PNM file");
    }

    pnm_info info;
    pnm_parser parser(data);

    if (!parser.parse_header(info)) {
        return decode_result::failure(decode_error::invalid_format, "Failed to parse PNM header");
    }

    // Check dimension limits
    const int max_w = options.max_width > 0 ? options.max_width : 16384;
    const int max_h = options.max_height > 0 ? options.max_height : 16384;
    if (info.width > max_w || info.height > max_h) {
        return decode_result::failure(decode_error::dimensions_exceeded, "Image dimensions exceed limits");
    }

    const auto width = static_cast<std::size_t>(info.width);
    const auto height = static_cast<std::size_t>(info.height);

    // All PNM formats are output as RGB
    if (!surf.set_size(info.width, info.height, pixel_format::rgb888)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    std::vector<std::uint8_t> row_buffer(width * 3);
    bool success = false;

    switch (info.type) {
        case PNM_TYPE_PBM_ASCII:
            success = decode_pbm_ascii(data, info.data_offset, width, height,
                                       row_buffer, surf);
            break;
        case PNM_TYPE_PGM_ASCII:
            success = decode_pgm_ascii(data, info.data_offset, width, height,
                                       info.maxval, row_buffer, surf);
            break;
        case PNM_TYPE_PPM_ASCII:
            success = decode_ppm_ascii(data, info.data_offset, width, height,
                                       info.maxval, row_buffer, surf);
            break;
        case PNM_TYPE_PBM_BINARY:
            success = decode_pbm_binary(data, info.data_offset, width, height,
                                        row_buffer, surf);
            break;
        case PNM_TYPE_PGM_BINARY:
            success = decode_pgm_binary(data, info.data_offset, width, height,
                                        info.maxval, row_buffer, surf);
            break;
        case PNM_TYPE_PPM_BINARY:
            success = decode_ppm_binary(data, info.data_offset, width, height,
                                        info.maxval, row_buffer, surf);
            break;
        default:
            return decode_result::failure(decode_error::unsupported_encoding,
                "Unsupported PNM type: P" + std::to_string(info.type));
    }

    if (!success) {
        return decode_result::failure(decode_error::truncated_data, "Failed to decode PNM pixel data");
    }

    return decode_result::success();
}

} // namespace onyx_image
