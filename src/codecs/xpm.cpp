#include <onyx_image/codecs/xpm.hpp>
#include "decode_helpers.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace onyx_image {

namespace {

struct rgba_color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

struct xpm_header {
    int width = 0;
    int height = 0;
    int colors = 0;
    int cpp = 0;
};

struct string_hash {
    using is_transparent = void;
    std::size_t operator()(std::string_view value) const noexcept {
        return std::hash<std::string_view>{}(value);
    }
    std::size_t operator()(const std::string& value) const noexcept {
        return operator()(std::string_view(value));
    }
};

struct string_equal {
    using is_transparent = void;
    bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
        return lhs == rhs;
    }
    bool operator()(const std::string& lhs, const std::string& rhs) const noexcept {
        return lhs == rhs;
    }
    bool operator()(const std::string& lhs, std::string_view rhs) const noexcept {
        return lhs == rhs;
    }
    bool operator()(std::string_view lhs, const std::string& rhs) const noexcept {
        return lhs == rhs;
    }
};

inline bool is_space(unsigned char c) {
    return std::isspace(c) != 0;
}

bool has_xpm_marker(std::span<const std::uint8_t> data) {
    const std::size_t limit = std::min<std::size_t>(data.size(), 512);
    for (std::size_t i = 0; i + 2 < limit; ++i) {
        char a = static_cast<char>(data[i]);
        char b = static_cast<char>(data[i + 1]);
        char c = static_cast<char>(data[i + 2]);
        if ((a == 'X' || a == 'x') && (b == 'P' || b == 'p') && (c == 'M' || c == 'm')) {
            return true;
        }
    }
    return false;
}

std::string_view ltrim_view(std::string_view line) {
    std::size_t pos = 0;
    while (pos < line.size() && is_space(static_cast<unsigned char>(line[pos]))) {
        pos++;
    }
    return line.substr(pos);
}

bool parse_header_line(std::string_view line, xpm_header& header) {
    int values[4] = {};
    std::size_t pos = 0;
    for (int idx = 0; idx < 4; ++idx) {
        while (pos < line.size() && is_space(static_cast<unsigned char>(line[pos]))) {
            pos++;
        }
        if (pos >= line.size()) {
            return false;
        }
        const char* start = line.data() + pos;
        const char* end = line.data() + line.size();
        int value = 0;
        auto result = std::from_chars(start, end, value);
        if (result.ec != std::errc{}) {
            return false;
        }
        values[idx] = value;
        pos = static_cast<std::size_t>(result.ptr - line.data());
    }

    header.width = values[0];
    header.height = values[1];
    header.colors = values[2];
    header.cpp = values[3];

    if (header.width <= 0 || header.height <= 0 || header.colors <= 0 || header.cpp <= 0) {
        return false;
    }

    return true;
}

std::string_view next_token(std::string_view line, std::size_t& pos) {
    while (pos < line.size() && is_space(static_cast<unsigned char>(line[pos]))) {
        pos++;
    }
    std::size_t start = pos;
    while (pos < line.size() && !is_space(static_cast<unsigned char>(line[pos]))) {
        pos++;
    }
    if (start == pos) {
        return {};
    }
    return line.substr(start, pos - start);
}

bool equals_ignore_case(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        unsigned char a = static_cast<unsigned char>(lhs[i]);
        unsigned char b = static_cast<unsigned char>(rhs[i]);
        if (std::tolower(a) != std::tolower(b)) {
            return false;
        }
    }
    return true;
}

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool parse_hex_component(std::string_view hex, unsigned int& out) {
    out = 0;
    if (hex.empty()) return false;
    for (char c : hex) {
        int v = hex_value(c);
        if (v < 0) return false;
        out = (out << 4) | static_cast<unsigned int>(v);
    }
    return true;
}

bool parse_named_color(std::string_view token, rgba_color& color) {
    std::string lowered;
    lowered.reserve(token.size());
    for (char c : token) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    if (lowered == "none" || lowered == "transparent") {
        color = {0, 0, 0, 0};
        return true;
    }

    struct named_color {
        const char* name;
        std::uint8_t r;
        std::uint8_t g;
        std::uint8_t b;
    };

    static constexpr named_color table[] = {
        {"black", 0, 0, 0},
        {"white", 255, 255, 255},
        {"red", 255, 0, 0},
        {"green", 0, 255, 0},
        {"blue", 0, 0, 255},
        {"yellow", 255, 255, 0},
        {"magenta", 255, 0, 255},
        {"cyan", 0, 255, 255},
        {"gray", 128, 128, 128},
        {"grey", 128, 128, 128},
        {"darkgray", 64, 64, 64},
        {"darkgrey", 64, 64, 64},
        {"lightgray", 192, 192, 192},
        {"lightgrey", 192, 192, 192},
    };

    for (const auto& entry : table) {
        if (lowered == entry.name) {
            color = {entry.r, entry.g, entry.b, 255};
            return true;
        }
    }

    auto parse_gray = [&](std::string_view prefix) -> bool {
        if (lowered.rfind(prefix, 0) != 0) {
            return false;
        }
        std::string_view digits(lowered.data() + prefix.size(), lowered.size() - prefix.size());
        if (digits.empty()) {
            color = {128, 128, 128, 255};
            return true;
        }
        int value = 0;
        auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value);
        if (result.ec != std::errc{} ||
            result.ptr != digits.data() + digits.size() ||
            value < 0 || value > 100) {
            return false;
        }
        std::uint8_t gray = static_cast<std::uint8_t>((value * 255 + 50) / 100);
        color = {gray, gray, gray, 255};
        return true;
    };

    if (parse_gray("gray") || parse_gray("grey")) {
        return true;
    }

    return false;
}

bool parse_color_token(std::string_view token, rgba_color& color) {
    if (token.empty()) {
        return false;
    }

    if (token[0] == '#') {
        std::string_view hex = token.substr(1);
        if (hex.size() == 3 || hex.size() == 4) {
            unsigned int r = 0;
            unsigned int g = 0;
            unsigned int b = 0;
            unsigned int a = 0xF;
            if (!parse_hex_component(hex.substr(0, 1), r) ||
                !parse_hex_component(hex.substr(1, 1), g) ||
                !parse_hex_component(hex.substr(2, 1), b)) {
                return false;
            }
            if (hex.size() == 4) {
                if (!parse_hex_component(hex.substr(3, 1), a)) {
                    return false;
                }
            }
            color = {static_cast<std::uint8_t>(r * 17),
                     static_cast<std::uint8_t>(g * 17),
                     static_cast<std::uint8_t>(b * 17),
                     static_cast<std::uint8_t>(a * 17)};
            return true;
        }
        if (hex.size() == 6 || hex.size() == 8) {
            unsigned int r = 0;
            unsigned int g = 0;
            unsigned int b = 0;
            unsigned int a = 255;
            if (!parse_hex_component(hex.substr(0, 2), r) ||
                !parse_hex_component(hex.substr(2, 2), g) ||
                !parse_hex_component(hex.substr(4, 2), b)) {
                return false;
            }
            if (hex.size() == 8) {
                if (!parse_hex_component(hex.substr(6, 2), a)) {
                    return false;
                }
            }
            color = {static_cast<std::uint8_t>(r),
                     static_cast<std::uint8_t>(g),
                     static_cast<std::uint8_t>(b),
                     static_cast<std::uint8_t>(a)};
            return true;
        }
        if (hex.size() == 12 || hex.size() == 16) {
            unsigned int r = 0;
            unsigned int g = 0;
            unsigned int b = 0;
            unsigned int a = 65535;
            if (!parse_hex_component(hex.substr(0, 4), r) ||
                !parse_hex_component(hex.substr(4, 4), g) ||
                !parse_hex_component(hex.substr(8, 4), b)) {
                return false;
            }
            if (hex.size() == 16) {
                if (!parse_hex_component(hex.substr(12, 4), a)) {
                    return false;
                }
            }
            color = {static_cast<std::uint8_t>(r / 257),
                     static_cast<std::uint8_t>(g / 257),
                     static_cast<std::uint8_t>(b / 257),
                     static_cast<std::uint8_t>(a / 257)};
            return true;
        }
        return false;
    }

    return parse_named_color(token, color);
}

bool parse_color_line(std::string_view line, int cpp, std::string& key, rgba_color& color) {
    if (cpp <= 0) {
        return false;
    }
    if (line.size() < static_cast<std::size_t>(cpp)) {
        return false;
    }

    key.assign(line.data(), static_cast<std::size_t>(cpp));

    std::string_view rest = line.substr(static_cast<std::size_t>(cpp));
    std::size_t pos = 0;
    std::string_view color_token;
    std::string_view fallback_token;

    while (pos < rest.size()) {
        std::string_view key_token = next_token(rest, pos);
        if (key_token.empty()) {
            break;
        }
        std::string_view value_token = next_token(rest, pos);
        if (value_token.empty()) {
            break;
        }
        if (equals_ignore_case(key_token, "c")) {
            if (color_token.empty()) {
                color_token = value_token;
            }
        } else if ((equals_ignore_case(key_token, "g") || equals_ignore_case(key_token, "m")) &&
                   fallback_token.empty()) {
            fallback_token = value_token;
        }
    }

    std::string_view chosen = !color_token.empty() ? color_token : fallback_token;
    if (chosen.empty()) {
        return false;
    }

    if (!parse_color_token(chosen, color)) {
        if (chosen == color_token && !fallback_token.empty()) {
            return parse_color_token(fallback_token, color);
        }
        return false;
    }

    return true;
}

bool append_escape(std::span<const std::uint8_t> data, std::size_t& pos, std::string& out) {
    if (pos >= data.size()) {
        return false;
    }
    char esc = static_cast<char>(data[pos++]);
    switch (esc) {
        case '\\':
        case '"':
            out.push_back(esc);
            return true;
        case 'n':
            out.push_back('\n');
            return true;
        case 'r':
            out.push_back('\r');
            return true;
        case 't':
            out.push_back('\t');
            return true;
        case 'v':
            out.push_back('\v');
            return true;
        case 'b':
            out.push_back('\b');
            return true;
        case 'f':
            out.push_back('\f');
            return true;
        case 'x': {
            unsigned int value = 0;
            int digits = 0;
            while (pos < data.size() && digits < 2) {
                int hv = hex_value(static_cast<char>(data[pos]));
                if (hv < 0) {
                    break;
                }
                value = (value << 4) | static_cast<unsigned int>(hv);
                pos++;
                digits++;
            }
            if (digits == 0) {
                out.push_back('x');
                return true;
            }
            out.push_back(static_cast<char>(value));
            return true;
        }
        default:
            break;
    }

    if (esc >= '0' && esc <= '7') {
        unsigned int value = static_cast<unsigned int>(esc - '0');
        int digits = 1;
        while (pos < data.size() && digits < 3) {
            char c = static_cast<char>(data[pos]);
            if (c < '0' || c > '7') {
                break;
            }
            value = (value << 3) | static_cast<unsigned int>(c - '0');
            pos++;
            digits++;
        }
        out.push_back(static_cast<char>(value));
        return true;
    }

    out.push_back(esc);
    return true;
}

bool extract_c_strings(std::span<const std::uint8_t> data,
                       std::vector<std::string>& lines,
                       std::size_t max_lines = 0) {
    std::size_t pos = 0;
    while (pos < data.size()) {
        if (data[pos] != '"') {
            pos++;
            continue;
        }
        pos++;
        std::string line;
        bool closed = false;
        while (pos < data.size()) {
            char c = static_cast<char>(data[pos++]);
            if (c == '"') {
                closed = true;
                break;
            }
            if (c == '\\') {
                if (!append_escape(data, pos, line)) {
                    return false;
                }
                continue;
            }
            line.push_back(c);
        }
        if (!closed) {
            return false;
        }
        lines.push_back(std::move(line));
        if (max_lines > 0 && lines.size() >= max_lines) {
            return true;
        }
    }
    return !lines.empty();
}

bool extract_raw_lines(std::span<const std::uint8_t> data,
                       std::vector<std::string>& lines,
                       std::size_t max_lines = 0) {
    std::size_t pos = 0;
    while (pos < data.size()) {
        std::size_t end = pos;
        while (end < data.size() && data[end] != '\n' && data[end] != '\r') {
            end++;
        }
        std::string line(reinterpret_cast<const char*>(data.data() + pos), end - pos);

        std::size_t next = end;
        if (next < data.size() && data[next] == '\r') {
            next++;
            if (next < data.size() && data[next] == '\n') {
                next++;
            }
        } else if (next < data.size() && data[next] == '\n') {
            next++;
        }
        pos = next;

        std::string_view trimmed = ltrim_view(line);
        if (trimmed.empty()) {
            continue;
        }
        if (trimmed[0] == '!') {
            continue;
        }
        if (equals_ignore_case(trimmed, "xpm2")) {
            continue;
        }

        lines.push_back(std::move(line));
        if (max_lines > 0 && lines.size() >= max_lines) {
            return true;
        }
    }
    return !lines.empty();
}

bool find_header_index(const std::vector<std::string>& lines, xpm_header& header, std::size_t& index) {
    for (std::size_t i = 0; i < lines.size(); ++i) {
        xpm_header candidate;
        if (!parse_header_line(lines[i], candidate)) {
            continue;
        }
        std::size_t remaining = lines.size() - (i + 1);
        std::size_t needed = static_cast<std::size_t>(candidate.colors) +
                             static_cast<std::size_t>(candidate.height);
        if (needed > remaining) {
            continue;
        }
        header = candidate;
        index = i;
        return true;
    }
    return false;
}

bool collect_xpm_lines(std::span<const std::uint8_t> data, std::vector<std::string>& lines) {
    std::vector<std::string> candidates;
    if (extract_c_strings(data, candidates)) {
        xpm_header header;
        std::size_t header_index = 0;
        if (find_header_index(candidates, header, header_index)) {
            lines = std::move(candidates);
            return true;
        }
    }

    candidates.clear();
    if (extract_raw_lines(data, candidates)) {
        xpm_header header;
        std::size_t header_index = 0;
        if (find_header_index(candidates, header, header_index)) {
            lines = std::move(candidates);
            return true;
        }
    }

    return false;
}

} // namespace

bool xpm_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    if (data.size() < 16) {
        return false;
    }
    if (!has_xpm_marker(data)) {
        return false;
    }

    std::vector<std::string> lines;
    if (!collect_xpm_lines(data, lines)) {
        return false;
    }

    xpm_header header;
    std::size_t header_index = 0;
    return find_header_index(lines, header, header_index);
}

decode_result xpm_decoder::decode(std::span<const std::uint8_t> data,
                                   surface& surf,
                                   const decode_options& options) {
    if (!has_xpm_marker(data)) {
        return decode_result::failure(decode_error::invalid_format, "Missing XPM marker");
    }

    std::vector<std::string> lines;
    if (!collect_xpm_lines(data, lines)) {
        return decode_result::failure(decode_error::invalid_format, "Invalid XPM data");
    }

    xpm_header header;
    std::size_t header_index = 0;
    if (!find_header_index(lines, header, header_index)) {
        return decode_result::failure(decode_error::invalid_format, "Invalid XPM header");
    }

    auto result = validate_dimensions(header.width, header.height, options);
    if (!result) {
        return result;
    }

    if (header.cpp <= 0) {
        return decode_result::failure(decode_error::invalid_format, "Invalid XPM character width");
    }

    const std::size_t width = static_cast<std::size_t>(header.width);
    const std::size_t height = static_cast<std::size_t>(header.height);
    const std::size_t cpp = static_cast<std::size_t>(header.cpp);

    if (width > std::numeric_limits<std::size_t>::max() / cpp) {
        return decode_result::failure(decode_error::invalid_format, "XPM row size overflow");
    }

    const std::size_t row_chars = width * cpp;

    const std::size_t colors_start = header_index + 1;
    const std::size_t pixels_start = colors_start + static_cast<std::size_t>(header.colors);
    if (pixels_start + height > lines.size()) {
        return decode_result::failure(decode_error::truncated_data, "XPM data truncated");
    }

    std::unordered_map<std::string, rgba_color, string_hash, string_equal> color_map;
    color_map.reserve(static_cast<std::size_t>(header.colors));
    bool has_alpha = false;

    for (std::size_t i = 0; i < static_cast<std::size_t>(header.colors); ++i) {
        const std::string& line = lines[colors_start + i];
        std::string key;
        rgba_color color;
        if (!parse_color_line(line, header.cpp, key, color)) {
            return decode_result::failure(decode_error::invalid_format, "Invalid XPM color table");
        }
        if (color.a < 255) {
            has_alpha = true;
        }
        color_map.emplace(std::move(key), color);
    }

    pixel_format format = has_alpha ? pixel_format::rgba8888 : pixel_format::rgb888;
    if (!surf.set_size(header.width, header.height, format)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    const std::size_t bytes_per_pixel = has_alpha ? 4 : 3;
    std::vector<std::uint8_t> row_buffer(width * bytes_per_pixel);

    for (std::size_t y = 0; y < height; ++y) {
        const std::string& line = lines[pixels_start + y];
        if (line.size() < row_chars) {
            return decode_result::failure(decode_error::truncated_data, "XPM pixel data truncated");
        }

        for (std::size_t x = 0; x < width; ++x) {
            std::size_t offset = x * cpp;
            std::string_view key_view(line.data() + offset, cpp);
            auto it = color_map.find(key_view);
            if (it == color_map.end()) {
                return decode_result::failure(decode_error::invalid_format, "Unknown XPM color key");
            }
            const auto& color = it->second;
            std::size_t dst = x * bytes_per_pixel;
            row_buffer[dst + 0] = color.r;
            row_buffer[dst + 1] = color.g;
            row_buffer[dst + 2] = color.b;
            if (has_alpha) {
                row_buffer[dst + 3] = color.a;
            }
        }

        surf.write_pixels(0, static_cast<int>(y),
                          static_cast<int>(row_buffer.size()),
                          row_buffer.data());
    }

    return decode_result::success();
}

} // namespace onyx_image
