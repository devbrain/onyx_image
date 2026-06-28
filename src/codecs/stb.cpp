// stb_image-based decoders for JPEG, TGA, GIF

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_PNG  // We use lodepng for PNG
#define STBI_NO_BMP  // Custom BMP support planned
#define STBI_NO_PSD
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM

#include <stb_image.h>

#include <onyx_image/codecs/jpeg.hpp>
#include <onyx_image/codecs/tga.hpp>
#include <onyx_image/codecs/gif.hpp>
#include "decode_helpers.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

namespace onyx_image {

namespace {

// Common stb_image decode helper
decode_result stb_decode_common(std::span<const std::uint8_t> data,
                                 surface& surf,
                                 const decode_options& options) {
    // Guard against data size exceeding INT_MAX (stb uses int for length)
    if (data.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return decode_result::failure(decode_error::truncated_data,
            "Input data exceeds maximum supported size");
    }

    // Pre-decode dimension check to avoid loading huge images
    int info_width = 0;
    int info_height = 0;
    int info_channels = 0;
    if (stbi_info_from_memory(data.data(), static_cast<int>(data.size()),
                              &info_width, &info_height, &info_channels)) {
        auto result = validate_dimensions(info_width, info_height, options);
        if (!result) return result;
    }

    int width = 0;
    int height = 0;
    int channels = 0;

    // stb_image only produces truecolor output, so color_output::source maps to
    // rgba8888 here; the indexed path is handled by the format-specific decoders
    // before this helper is reached. color_output::rgb requests 3 channels.
    const bool want_rgb = options.output == color_output::rgb;
    const int desired_channels = want_rgb ? 3 : 4;
    const pixel_format out_format = want_rgb ? pixel_format::rgb888 : pixel_format::rgba8888;

    stbi_uc* pixels = stbi_load_from_memory(
        data.data(),
        static_cast<int>(data.size()),
        &width,
        &height,
        &channels,
        desired_channels
    );

    if (!pixels) {
        return decode_result::failure(decode_error::invalid_format, stbi_failure_reason());
    }

    // Use unique_ptr for automatic cleanup
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixel_guard(pixels, stbi_image_free);

    // Post-decode dimension check (fallback if stbi_info_from_memory failed)
    auto result = validate_dimensions(width, height, options);
    if (!result) return result;

    if (!surf.set_size(width, height, out_format)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    // Copy pixel data to surface
    write_rows(surf, pixels, static_cast<std::size_t>(width) * desired_channels, height);

    return decode_result::success();
}

// ----------------------------------------------------------------------------
// GIF logical-screen repair
//
// Some DOS-era GIF encoders write a Logical Screen Descriptor with width and/or
// height set to 0 and store the real dimensions only in the first Image
// Descriptor. stb_image enforces that every frame fits within the logical
// screen, so it rejects these files with "bad Image Descriptor". Lenient
// decoders (PIL, ImageMagick) derive the canvas from the image descriptors
// instead. We do the same: scan the frames, compute the bounding canvas, and
// patch the zero field(s) on a private copy before handing it to stb_image.
// ----------------------------------------------------------------------------

constexpr int gif_read_u16le(std::span<const std::uint8_t> data, std::size_t off) noexcept {
    return static_cast<int>(data[off]) | (static_cast<int>(data[off + 1]) << 8);
}

// Skip a chain of size-prefixed GIF sub-blocks, returning the position just
// past the terminating zero-length block.
std::size_t gif_skip_sub_blocks(std::span<const std::uint8_t> data, std::size_t pos) noexcept {
    while (pos < data.size()) {
        const std::uint8_t n = data[pos++];
        if (n == 0) break;
        pos += n;
    }
    return pos;
}

// Walk every Image Descriptor and return the canvas size required to contain
// all frames (max x+w, max y+h). Returns {0, 0} if none are found.
struct gif_extent { int w; int h; };

gif_extent gif_required_canvas(std::span<const std::uint8_t> data) noexcept {
    constexpr std::size_t header_size = 13; // signature(6) + LSD(7)
    if (data.size() < header_size) return {0, 0};

    std::size_t pos = header_size;
    const std::uint8_t packed = data[10];
    if (packed & 0x80) { // global color table present
        pos += (std::size_t{2} << (packed & 0x07)) * 3;
    }

    int max_w = 0;
    int max_h = 0;
    while (pos < data.size()) {
        const std::uint8_t block = data[pos++];
        if (block == 0x3B) { // trailer
            break;
        } else if (block == 0x21) { // extension
            if (pos >= data.size()) break;
            ++pos; // label
            pos = gif_skip_sub_blocks(data, pos);
        } else if (block == 0x2C) { // image descriptor
            if (pos + 9 > data.size()) break;
            const int x = gif_read_u16le(data, pos + 0);
            const int y = gif_read_u16le(data, pos + 2);
            const int w = gif_read_u16le(data, pos + 4);
            const int h = gif_read_u16le(data, pos + 6);
            const std::uint8_t img_packed = data[pos + 8];
            pos += 9;
            max_w = std::max(max_w, x + w);
            max_h = std::max(max_h, y + h);
            if (img_packed & 0x80) { // local color table present
                pos += (std::size_t{2} << (img_packed & 0x07)) * 3;
            }
            if (pos >= data.size()) break;
            ++pos; // LZW minimum code size
            pos = gif_skip_sub_blocks(data, pos);
        } else {
            break; // unexpected byte; stop scanning
        }
    }
    return {max_w, max_h};
}

// If the logical screen width/height is zero, return a patched copy of the GIF
// with the field(s) filled in from the image descriptors. Returns nullopt when
// no patch is needed or a sensible canvas could not be derived (in which case
// the original data is handed to stb_image unchanged).
std::optional<std::vector<std::uint8_t>> gif_patch_zero_screen(std::span<const std::uint8_t> data) {
    if (data.size() < 13) return std::nullopt;

    const bool zero_w = gif_read_u16le(data, 6) == 0;
    const bool zero_h = gif_read_u16le(data, 8) == 0;
    if (!zero_w && !zero_h) return std::nullopt;

    const gif_extent canvas = gif_required_canvas(data);
    const bool need_w = zero_w && (canvas.w <= 0 || canvas.w > 0xFFFF);
    const bool need_h = zero_h && (canvas.h <= 0 || canvas.h > 0xFFFF);
    if (need_w || need_h) return std::nullopt; // can't fix; let stb report it

    std::vector<std::uint8_t> copy(data.begin(), data.end());
    if (zero_w) {
        copy[6] = static_cast<std::uint8_t>(canvas.w & 0xFF);
        copy[7] = static_cast<std::uint8_t>((canvas.w >> 8) & 0xFF);
    }
    if (zero_h) {
        copy[8] = static_cast<std::uint8_t>(canvas.h & 0xFF);
        copy[9] = static_cast<std::uint8_t>((canvas.h >> 8) & 0xFF);
    }
    return copy;
}

// ----------------------------------------------------------------------------
// Source-palette metadata
//
// stb_image always expands paletted formats to truecolor, discarding the
// original color table. We parse it ourselves and attach it to the surface via
// write_palette so callers can reach the source palette even though the pixels
// are RGBA. The pixel format is left untouched (Phase 1: palette as metadata).
// ----------------------------------------------------------------------------

// Attach a GIF's global color table to the surface. No-op if the file has no
// global color table (e.g. it relies solely on per-frame local tables).
void gif_attach_palette(std::span<const std::uint8_t> d, surface& surf) {
    if (d.size() < 13) return;
    const std::uint8_t packed = d[10];
    if (!(packed & 0x80)) return;                       // no global color table
    const int entries = 2 << (packed & 0x07);           // 2^(N+1)
    const std::size_t bytes = static_cast<std::size_t>(entries) * 3;
    if (13 + bytes > d.size()) return;
    surf.set_palette_size(entries);
    surf.write_palette(0, d.subspan(13, bytes));        // GIF stores RGB triplets
}

// Attach a colormapped TGA's palette to the surface, converting the stored
// little-endian BGR(A) entries to the RGB888 triplets onyx_image uses. No-op
// for non-colormapped TGAs.
void tga_attach_palette(std::span<const std::uint8_t> d, surface& surf) {
    if (d.size() < 18) return;
    if (d[1] != 1) return;                              // color map type: 1 = present
    const int first  = d[3] | (d[4] << 8);             // first entry index
    const int length = d[5] | (d[6] << 8);             // number of entries
    const int entry_bits = d[7];
    if (length <= 0 || first < 0 || first + length > 256) return;
    if (entry_bits != 15 && entry_bits != 16 && entry_bits != 24 && entry_bits != 32) return;

    const int entry_bytes = (entry_bits + 7) / 8;
    const std::size_t cmap_off = std::size_t{18} + d[0]; // skip header + image ID field
    const std::size_t cmap_bytes = static_cast<std::size_t>(length) * entry_bytes;
    if (cmap_off + cmap_bytes > d.size()) return;

    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(length) * 3);
    for (int i = 0; i < length; ++i) {
        const std::uint8_t* p = d.data() + cmap_off + static_cast<std::size_t>(i) * entry_bytes;
        std::uint8_t r, g, b;
        if (entry_bytes == 2) {                         // 15/16-bit: (A)RRRRRGGGGGBBBBB, LE
            const unsigned v = static_cast<unsigned>(p[0]) | (static_cast<unsigned>(p[1]) << 8);
            const auto exp5 = [](unsigned c) {
                return static_cast<std::uint8_t>((c << 3) | (c >> 2));
            };
            r = exp5((v >> 10) & 0x1F);
            g = exp5((v >> 5) & 0x1F);
            b = exp5(v & 0x1F);
        } else {                                        // 24/32-bit: B, G, R, (A)
            b = p[0];
            g = p[1];
            r = p[2];
        }
        rgb[static_cast<std::size_t>(i) * 3 + 0] = r;
        rgb[static_cast<std::size_t>(i) * 3 + 1] = g;
        rgb[static_cast<std::size_t>(i) * 3 + 2] = b;
    }
    surf.set_palette_size(first + length);
    surf.write_palette(first, rgb);
}

// ----------------------------------------------------------------------------
// Native indexed GIF decoding (color_output::source)
//
// stb_image only yields expanded RGBA, so to preserve the indexed
// representation we decode the GIF's first image frame ourselves: GIF-LZW
// decompress, honor interlacing, and carry the global/local color table plus
// the transparent index. The RGBA/RGB paths still go through stb_image.
// ----------------------------------------------------------------------------

struct gif_frame {
    int w = 0;
    int h = 0;
    std::vector<std::uint8_t> indices;    // w*h palette indices
    std::vector<std::uint8_t> palette;    // RGB triplets
    int palette_count = 0;
    int transparent_index = -1;
};

// Decode a single GIF-LZW code stream into `expected` index bytes.
bool gif_lzw_decode(std::span<const std::uint8_t> in, int min_code_size,
                    std::size_t expected, std::vector<std::uint8_t>& out) {
    if (min_code_size < 2 || min_code_size > 8) return false;
    constexpr int MAX_CODES = 4096;
    const int clear_code = 1 << min_code_size;
    const int end_code = clear_code + 1;

    std::array<int, MAX_CODES> prefix{};
    std::array<std::uint8_t, MAX_CODES> suffix{};
    std::array<std::uint8_t, MAX_CODES> stack{};
    for (int i = 0; i < MAX_CODES; ++i) {
        prefix[i] = -1;
        suffix[i] = static_cast<std::uint8_t>(i < clear_code ? i : 0);
    }

    out.clear();
    out.reserve(expected);

    int code_size = min_code_size + 1;
    int next_code = end_code + 1;
    int prev = -1;
    int first = 0;

    std::size_t bit_pos = 0;
    const std::size_t total_bits = in.size() * 8;
    auto read_code = [&](int bits) -> int {
        if (bit_pos + static_cast<std::size_t>(bits) > total_bits) return -1;
        int value = 0;
        for (int i = 0; i < bits; ++i) {
            const std::size_t bp = bit_pos + static_cast<std::size_t>(i);
            value |= ((in[bp >> 3] >> (bp & 7)) & 1) << i; // GIF packs codes LSB-first
        }
        bit_pos += static_cast<std::size_t>(bits);
        return value;
    };

    while (out.size() < expected) {
        const int code = read_code(code_size);
        if (code < 0) break;                       // ran out of bits
        if (code == clear_code) {
            code_size = min_code_size + 1;
            next_code = end_code + 1;
            prev = -1;
            continue;
        }
        if (code == end_code) break;

        if (prev == -1) {                          // first symbol after a clear
            if (code >= clear_code) return false;
            first = code;
            out.push_back(static_cast<std::uint8_t>(code));
            prev = code;
            continue;
        }

        int sp = 0;
        int cur;
        if (code < next_code) {
            cur = code;
        } else if (code == next_code) {            // KwKwK special case
            stack[sp++] = static_cast<std::uint8_t>(first);
            cur = prev;
        } else {
            return false;                          // code out of range
        }

        while (cur >= clear_code) {                // unwind to root
            if (cur >= MAX_CODES || sp >= MAX_CODES) return false;
            stack[sp++] = suffix[cur];
            cur = prefix[cur];
            if (cur < 0) return false;
        }
        first = suffix[cur];
        stack[sp++] = static_cast<std::uint8_t>(first);

        while (sp > 0 && out.size() < expected) out.push_back(stack[--sp]);

        if (next_code < MAX_CODES) {               // add prev + first to the table
            prefix[next_code] = prev;
            suffix[next_code] = static_cast<std::uint8_t>(first);
            ++next_code;
            if (next_code == (1 << code_size) && code_size < 12) ++code_size;
        }
        prev = code;
    }
    return out.size() == expected;
}

// Reorder interlaced GIF rows (passes 8/8/4/2 starting at 0/4/2/1) into linear order.
void gif_deinterlace(std::vector<std::uint8_t>& idx, int w, int h) {
    std::vector<std::uint8_t> linear(static_cast<std::size_t>(w) * h);
    const int starts[4] = {0, 4, 2, 1};
    const int steps[4]  = {8, 8, 4, 2};
    int src_row = 0;
    for (int pass = 0; pass < 4; ++pass) {
        for (int row = starts[pass]; row < h; row += steps[pass]) {
            std::memcpy(&linear[static_cast<std::size_t>(row) * w],
                        &idx[static_cast<std::size_t>(src_row) * w],
                        static_cast<std::size_t>(w));
            ++src_row;
        }
    }
    idx.swap(linear);
}

// Decode the first image of a GIF into indices + palette. Returns false on any
// structural problem, letting the caller fall back to stb's RGBA path.
bool gif_decode_first_frame(std::span<const std::uint8_t> d, gif_frame& out) {
    if (d.size() < 13) return false;
    const std::uint8_t screen_packed = d[10];
    std::size_t pos = 13;

    std::span<const std::uint8_t> gct;
    int gct_count = 0;
    if (screen_packed & 0x80) {
        gct_count = 2 << (screen_packed & 0x07);
        const std::size_t bytes = static_cast<std::size_t>(gct_count) * 3;
        if (pos + bytes > d.size()) return false;
        gct = d.subspan(pos, bytes);
        pos += bytes;
    }

    int transparent_index = -1;
    while (pos < d.size()) {
        const std::uint8_t block = d[pos++];
        if (block == 0x3B) return false;           // trailer before any image
        if (block == 0x21) {                       // extension
            if (pos >= d.size()) return false;
            const std::uint8_t label = d[pos++];
            if (label == 0xF9 && pos < d.size() && d[pos] >= 4 && pos + 1 + d[pos] <= d.size()) {
                const std::uint8_t flags = d[pos + 1];
                if (flags & 0x01) transparent_index = d[pos + 4];
            }
            pos = gif_skip_sub_blocks(d, pos);
            continue;
        }
        if (block != 0x2C) return false;           // unexpected

        // Image descriptor: x(2) y(2) w(2) h(2) packed(1)
        if (pos + 9 > d.size()) return false;
        const int w = gif_read_u16le(d, pos + 4);
        const int h = gif_read_u16le(d, pos + 6);
        const std::uint8_t img_packed = d[pos + 8];
        pos += 9;
        if (w <= 0 || h <= 0) return false;

        std::span<const std::uint8_t> lct;
        int lct_count = 0;
        if (img_packed & 0x80) {                   // local color table
            lct_count = 2 << (img_packed & 0x07);
            const std::size_t bytes = static_cast<std::size_t>(lct_count) * 3;
            if (pos + bytes > d.size()) return false;
            lct = d.subspan(pos, bytes);
            pos += bytes;
        }
        const std::span<const std::uint8_t> palette = lct_count ? lct : gct;
        const int palette_count = lct_count ? lct_count : gct_count;
        if (palette_count == 0) return false;      // nothing to index against

        // LZW data: min code size, then size-prefixed sub-blocks.
        if (pos >= d.size()) return false;
        const int min_code_size = d[pos++];
        std::vector<std::uint8_t> lzw;
        while (pos < d.size()) {
            const std::uint8_t n = d[pos++];
            if (n == 0) break;
            if (pos + n > d.size()) return false;
            lzw.insert(lzw.end(), d.begin() + static_cast<std::ptrdiff_t>(pos),
                       d.begin() + static_cast<std::ptrdiff_t>(pos + n));
            pos += n;
        }

        const std::size_t expected = static_cast<std::size_t>(w) * h;
        std::vector<std::uint8_t> indices;
        if (!gif_lzw_decode(lzw, min_code_size, expected, indices)) return false;
        if (img_packed & 0x40) gif_deinterlace(indices, w, h);

        out.w = w;
        out.h = h;
        out.indices = std::move(indices);
        out.palette.assign(palette.begin(), palette.end());
        out.palette_count = palette_count;
        out.transparent_index = transparent_index;
        return true;
    }
    return false;
}

} // namespace

// ============================================================================
// JPEG Decoder
// ============================================================================

bool jpeg_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    // JPEG starts with FFD8FF
    if (data.size() < 3) {
        return false;
    }
    return data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF;
}

decode_result jpeg_decoder::decode(std::span<const std::uint8_t> data,
                                    surface& surf,
                                    const decode_options& options) {
    if (!sniff(data)) {
        return decode_result::failure(decode_error::invalid_format, "Not a valid JPEG file");
    }
    return stb_decode_common(data, surf, options);
}

// ============================================================================
// TGA Decoder
// ============================================================================

bool tga_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    // TGA has no magic number, but we can check for valid header fields
    // Minimum TGA header is 18 bytes
    if (data.size() < 18) {
        return false;
    }

    // Check image type (byte 2): valid values are 0-3, 9-11
    const std::uint8_t image_type = data[2];
    if (image_type > 11 || (image_type > 3 && image_type < 9)) {
        return false;
    }

    // Check color map type (byte 1): must be 0 or 1
    const std::uint8_t colormap_type = data[1];
    if (colormap_type > 1) {
        return false;
    }

    // Check bits per pixel (byte 16): valid values are 8, 15, 16, 24, 32
    const std::uint8_t bpp = data[16];
    if (bpp != 8 && bpp != 15 && bpp != 16 && bpp != 24 && bpp != 32) {
        return false;
    }

    // Check if dimensions are reasonable
    const int width = data[12] | (data[13] << 8);
    const int height = data[14] | (data[15] << 8);
    if (width == 0 || height == 0 || width > 32768 || height > 32768) {
        return false;
    }

    return true;
}

decode_result tga_decoder::decode(std::span<const std::uint8_t> data,
                                   surface& surf,
                                   const decode_options& options) {
    if (!sniff(data)) {
        return decode_result::failure(decode_error::invalid_format, "Not a valid TGA file");
    }
    auto result = stb_decode_common(data, surf, options);
    if (result) {
        tga_attach_palette(data, surf); // expose source palette as metadata
    }
    return result;
}

// ============================================================================
// GIF Decoder
// ============================================================================

bool gif_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    // GIF starts with "GIF87a" or "GIF89a"
    if (data.size() < 6) {
        return false;
    }
    return data[0] == 'G' && data[1] == 'I' && data[2] == 'F' &&
           data[3] == '8' && (data[4] == '7' || data[4] == '9') && data[5] == 'a';
}

decode_result gif_decoder::decode(std::span<const std::uint8_t> data,
                                   surface& surf,
                                   const decode_options& options) {
    if (!sniff(data)) {
        return decode_result::failure(decode_error::invalid_format, "Not a valid GIF file");
    }

    // Preserve the indexed representation when the caller asks for source output.
    // Falls back to the stb (RGBA) path if the native decode can't handle the file.
    if (options.output == color_output::source) {
        gif_frame frame;
        if (gif_decode_first_frame(data, frame)) {
            if (auto dim = validate_dimensions(frame.w, frame.h, options); !dim) return dim;
            if (!surf.set_size(frame.w, frame.h, pixel_format::indexed8)) {
                return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
            }
            surf.set_palette_size(frame.palette_count);
            surf.write_palette(0, frame.palette);
            if (frame.transparent_index >= 0) surf.set_transparent_index(frame.transparent_index);
            for (int y = 0; y < frame.h; ++y) {
                surf.write_pixels(0, y, frame.w, &frame.indices[static_cast<std::size_t>(y) * frame.w]);
            }
            return decode_result::success();
        }
        // fall through to stb expansion on any structural problem
    }

    // Repair GIFs that declare a 0-sized logical screen (see gif_patch_zero_screen).
    auto patched = gif_patch_zero_screen(data);
    const std::span<const std::uint8_t> input = patched ? std::span<const std::uint8_t>(*patched) : data;

    auto result = stb_decode_common(input, surf, options);
    if (result) {
        gif_attach_palette(input, surf); // expose source palette as metadata
    }
    return result;
}

} // namespace onyx_image
