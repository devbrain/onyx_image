#include <onyx_image/codecs/lbm.hpp>
#include <formats/lbm/lbm.hh>

#include <iff/parser.hh>

#include <algorithm>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace onyx_image {

namespace {

constexpr std::uint32_t CAMG_HAM_FLAG = 0x0800;
constexpr std::uint32_t CAMG_EHB_FLAG = 0x0080;

constexpr std::uint8_t MASKING_NONE = 0;
constexpr std::uint8_t MASKING_HAS_MASK = 1;
constexpr std::uint8_t MASKING_HAS_TRANSPARENT_COLOR = 2;
constexpr std::uint8_t MASKING_LASSO = 3;

constexpr std::uint8_t COMPRESSION_NONE = 0;
constexpr std::uint8_t COMPRESSION_BYTERUN = 1;

// IFF signature: "FORM"
constexpr std::uint8_t IFF_SIGNATURE[] = {'F', 'O', 'R', 'M'};

struct lbm_parse_result {
    std::optional<iff::fourcc> form_type;
    std::optional<formats::lbm::bmhd> bmhd;
    std::optional<formats::lbm::viewport_mode> camg;
    std::vector<formats::lbm::color_register> cmap;
    std::vector<std::uint8_t> body;
};

bool unpack_byterun1(const std::uint8_t*& src, const std::uint8_t* end,
                     std::uint8_t* dst, std::size_t expected) {
    std::size_t produced = 0;
    while (produced < expected) {
        if (src >= end) {
            return false;
        }
        const auto control = static_cast<std::int8_t>(*src++);
        if (control >= 0) {
            const std::size_t count = static_cast<std::size_t>(control) + 1;
            if (src + count > end || produced + count > expected) {
                return false;
            }
            std::memcpy(dst + produced, src, count);
            src += count;
            produced += count;
        } else if (control != -128) {
            const std::size_t count = static_cast<std::size_t>(-control) + 1;
            if (src >= end || produced + count > expected) {
                return false;
            }
            std::fill_n(dst + produced, count, *src++);
            produced += count;
        }
    }
    return true;
}

bool advance_byterun1(const std::uint8_t*& src, const std::uint8_t* end,
                      std::size_t expected) {
    std::size_t produced = 0;
    while (produced < expected) {
        if (src >= end) {
            return false;
        }
        const auto control = static_cast<std::int8_t>(*src++);
        if (control >= 0) {
            const std::size_t count = static_cast<std::size_t>(control) + 1;
            if (src + count > end || produced + count > expected) {
                return false;
            }
            src += count;
            produced += count;
        } else if (control != -128) {
            const std::size_t count = static_cast<std::size_t>(-control) + 1;
            if (src >= end || produced + count > expected) {
                return false;
            }
            src += 1;
            produced += count;
        }
    }
    return true;
}

bool can_decode_byterun(std::span<const std::uint8_t> body,
                        std::size_t bytes_per_row,
                        std::size_t stored_planes,
                        std::size_t height,
                        bool per_plane_rows) {
    const std::uint8_t* src = body.data();
    const std::uint8_t* end = body.data() + body.size();
    for (std::size_t y = 0; y < height; ++y) {
        if (per_plane_rows) {
            for (std::size_t p = 0; p < stored_planes; ++p) {
                if (!advance_byterun1(src, end, bytes_per_row)) {
                    return false;
                }
            }
        } else {
            const std::size_t row_bytes = bytes_per_row * stored_planes;
            if (!advance_byterun1(src, end, row_bytes)) {
                return false;
            }
        }
    }
    return true;
}

std::vector<std::uint8_t> build_palette_rgb(const std::vector<formats::lbm::color_register>& cmap,
                                             std::size_t count) {
    std::vector<std::uint8_t> palette(count * 3);
    for (std::size_t i = 0; i < count; ++i) {
        if (i < cmap.size()) {
            palette[i * 3 + 0] = cmap[i].red;
            palette[i * 3 + 1] = cmap[i].green;
            palette[i * 3 + 2] = cmap[i].blue;
        } else {
            const std::uint8_t value = count > 1
                ? static_cast<std::uint8_t>((i * 255u) / (count - 1))
                : static_cast<std::uint8_t>(0);
            palette[i * 3 + 0] = value;
            palette[i * 3 + 1] = value;
            palette[i * 3 + 2] = value;
        }
    }
    return palette;
}

std::vector<std::uint8_t> build_ehb_palette(const std::vector<formats::lbm::color_register>& cmap) {
    auto base = build_palette_rgb(cmap, 32);
    std::vector<std::uint8_t> palette(64 * 3);

    // Copy base palette
    std::memcpy(palette.data(), base.data(), 32 * 3);

    // Create half-brightness versions
    for (std::size_t i = 0; i < 32; ++i) {
        palette[(32 + i) * 3 + 0] = base[i * 3 + 0] >> 1;
        palette[(32 + i) * 3 + 1] = base[i * 3 + 1] >> 1;
        palette[(32 + i) * 3 + 2] = base[i * 3 + 2] >> 1;
    }
    return palette;
}

lbm_parse_result parse_lbm_chunks(std::span<const std::uint8_t> data) {
    lbm_parse_result result{};

    std::string buffer(reinterpret_cast<const char*>(data.data()), data.size());
    std::istringstream stream(buffer, std::ios::binary);

    using iff::operator""_4cc;
    constexpr auto form_ilbm = "ILBM"_4cc;
    constexpr auto form_pbm = "PBM "_4cc;
    constexpr auto chunk_bmhd = "BMHD"_4cc;
    constexpr auto chunk_cmap = "CMAP"_4cc;
    constexpr auto chunk_camg = "CAMG"_4cc;
    constexpr auto chunk_body = "BODY"_4cc;

    iff::handler_registry handlers;
    auto handle_chunk = [&](const iff::chunk_event& event) {
        if (event.type != iff::chunk_event_type::begin || !event.reader) {
            return;
        }

        if (event.current_form) {
            result.form_type = event.current_form;
        }

        const auto bytes = event.reader->read_all();
        const auto* ptr = reinterpret_cast<const std::uint8_t*>(bytes.data());
        const auto* end = ptr + bytes.size();

        try {
            if (event.header.id == chunk_bmhd) {
                result.bmhd = formats::lbm::bmhd::read(ptr, end);
            } else if (event.header.id == chunk_cmap) {
                const std::size_t count = bytes.size() / 3;
                result.cmap.reserve(result.cmap.size() + count);
                for (std::size_t i = 0; i < count; ++i) {
                    result.cmap.push_back(formats::lbm::color_register::read(ptr, end));
                }
            } else if (event.header.id == chunk_camg) {
                result.camg = formats::lbm::viewport_mode::read(ptr, end);
            } else if (event.header.id == chunk_body) {
                result.body.resize(bytes.size());
                if (!bytes.empty()) {
                    std::memcpy(result.body.data(), bytes.data(), bytes.size());
                }
            }
        } catch (...) {
            // Ignore parse errors for individual chunks
        }
    };

    auto register_chunks = [&](iff::fourcc form) {
        handlers.on_chunk_in_form(form, chunk_bmhd, handle_chunk);
        handlers.on_chunk_in_form(form, chunk_cmap, handle_chunk);
        handlers.on_chunk_in_form(form, chunk_camg, handle_chunk);
        handlers.on_chunk_in_form(form, chunk_body, handle_chunk);
    };

    register_chunks(form_ilbm);
    register_chunks(form_pbm);

    iff::parse(stream, handlers);
    return result;
}

} // namespace

bool lbm_decoder::sniff(std::span<const std::uint8_t> data) noexcept {
    // Minimum IFF header: "FORM" + size (4) + type (4) = 12 bytes
    if (data.size() < 12) {
        return false;
    }

    // Check for "FORM" signature
    for (std::size_t i = 0; i < 4; ++i) {
        if (data[i] != IFF_SIGNATURE[i]) {
            return false;
        }
    }

    // Check for ILBM or PBM form type at offset 8
    const bool is_ilbm = (data[8] == 'I' && data[9] == 'L' && data[10] == 'B' && data[11] == 'M');
    const bool is_pbm = (data[8] == 'P' && data[9] == 'B' && data[10] == 'M' && data[11] == ' ');

    return is_ilbm || is_pbm;
}

decode_result lbm_decoder::decode(std::span<const std::uint8_t> data,
                                   surface& surf,
                                   const decode_options& options) {
    if (!sniff(data)) {
        return decode_result::failure(decode_error::invalid_format, "Not a valid IFF ILBM/PBM file");
    }

    lbm_parse_result parsed;
    try {
        parsed = parse_lbm_chunks(data);
    } catch (const std::exception& e) {
        return decode_result::failure(decode_error::invalid_format, e.what());
    }

    if (!parsed.bmhd) {
        return decode_result::failure(decode_error::invalid_format, "Missing BMHD chunk");
    }

    if (parsed.body.empty()) {
        return decode_result::failure(decode_error::invalid_format, "Missing BODY chunk");
    }

    using iff::operator""_4cc;
    const bool is_pbm = parsed.form_type && *parsed.form_type == "PBM "_4cc;
    const bool is_ilbm = parsed.form_type && *parsed.form_type == "ILBM"_4cc;

    if (parsed.form_type && !is_ilbm && !is_pbm) {
        return decode_result::failure(decode_error::invalid_format, "Unknown IFF form type");
    }

    const auto& header = *parsed.bmhd;
    if (header.num_planes == 0) {
        return decode_result::failure(decode_error::invalid_format, "Invalid number of planes");
    }

    const std::uint8_t masking_value = static_cast<std::uint8_t>(header.masking);
    const std::uint8_t compression_value = static_cast<std::uint8_t>(header.compression);
    const bool has_mask = masking_value == MASKING_HAS_MASK;

    if (compression_value != COMPRESSION_NONE && compression_value != COMPRESSION_BYTERUN) {
        return decode_result::failure(decode_error::unsupported_encoding, "Unsupported compression");
    }

    const int width = header.width;
    const int height = header.height;

    // Check dimension limits
    const int max_w = options.max_width > 0 ? options.max_width : 16384;
    const int max_h = options.max_height > 0 ? options.max_height : 16384;

    if (width > max_w || height > max_h) {
        return decode_result::failure(decode_error::dimensions_exceeded, "Image dimensions exceed limits");
    }

    // Handle PBM (chunky) format
    if (is_pbm) {
        if (masking_value != 0) {
            return decode_result::failure(decode_error::unsupported_encoding, "PBM with masking not supported");
        }

        const std::size_t bytes_per_row = static_cast<std::size_t>(width);

        if (!surf.set_size(width, height, pixel_format::indexed8)) {
            return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
        }

        const std::size_t palette_size = static_cast<std::size_t>(1u) << header.num_planes;
        auto palette = build_palette_rgb(parsed.cmap, palette_size);
        surf.set_palette_size(static_cast<int>(palette_size));
        surf.write_palette(0, palette);

        const std::uint8_t* src = parsed.body.data();
        const std::uint8_t* src_end = parsed.body.data() + parsed.body.size();
        std::vector<std::uint8_t> row_buffer(bytes_per_row);

        for (int y = 0; y < height; ++y) {
            if (compression_value == COMPRESSION_NONE) {
                if (src + bytes_per_row > src_end) {
                    return decode_result::failure(decode_error::truncated_data, "Unexpected end of data");
                }
                surf.write_pixels(0, y, static_cast<int>(bytes_per_row), src);
                src += bytes_per_row;
            } else {
                if (!unpack_byterun1(src, src_end, row_buffer.data(), bytes_per_row)) {
                    return decode_result::failure(decode_error::truncated_data, "ByteRun1 decode failed");
                }
                surf.write_pixels(0, y, static_cast<int>(bytes_per_row), row_buffer.data());
            }
        }

        return decode_result::success();
    }

    // Handle ILBM (planar) format
    const bool is_truecolor = header.num_planes == 24 || header.num_planes == 32;
    if (header.num_planes > 8 && !is_truecolor) {
        return decode_result::failure(decode_error::unsupported_bit_depth, "Unsupported bit depth");
    }

    const std::size_t bytes_per_row = ((static_cast<std::size_t>(width) + 15) / 16) * 2;
    const std::size_t plane_count = header.num_planes;
    const std::size_t stored_planes = plane_count + (has_mask ? 1 : 0);

    const bool byterun_per_plane = compression_value == COMPRESSION_BYTERUN &&
                                   can_decode_byterun(parsed.body, bytes_per_row, stored_planes,
                                                     static_cast<std::size_t>(height), true);
    const bool byterun_per_scanline = compression_value == COMPRESSION_BYTERUN &&
                                      can_decode_byterun(parsed.body, bytes_per_row, stored_planes,
                                                        static_cast<std::size_t>(height), false);

    if (compression_value == COMPRESSION_BYTERUN && !byterun_per_plane && !byterun_per_scanline) {
        return decode_result::failure(decode_error::truncated_data, "Invalid ByteRun1 data");
    }

    // Determine output format
    const bool ham_mode = parsed.camg && ((parsed.camg->mode & CAMG_HAM_FLAG) != 0);
    const bool ehb_mode = parsed.camg && ((parsed.camg->mode & CAMG_EHB_FLAG) != 0);

    pixel_format out_format;
    if (is_truecolor || ham_mode) {
        out_format = pixel_format::rgba8888;
    } else {
        out_format = pixel_format::indexed8;
    }

    if (!surf.set_size(width, height, out_format)) {
        return decode_result::failure(decode_error::internal_error, "Failed to allocate surface");
    }

    // Prepare palette for indexed modes
    std::vector<std::uint8_t> palette;
    if (!is_truecolor && !ham_mode) {
        if (ehb_mode && plane_count == 6) {
            palette = build_ehb_palette(parsed.cmap);
            surf.set_palette_size(64);
        } else {
            const std::size_t palette_size = static_cast<std::size_t>(1u) << plane_count;
            palette = build_palette_rgb(parsed.cmap, palette_size);
            surf.set_palette_size(static_cast<int>(palette_size));
        }
        surf.write_palette(0, palette);
    }

    // Decode planar data
    std::vector<std::uint8_t> row_data(bytes_per_row * stored_planes);
    std::vector<std::uint8_t> indices(static_cast<std::size_t>(width));
    const std::uint8_t* src = parsed.body.data();
    const std::uint8_t* src_end = parsed.body.data() + parsed.body.size();

    // For HAM mode, we need the base palette
    std::vector<std::uint8_t> ham_base_palette;
    if (ham_mode) {
        const std::size_t base_size = plane_count == 6 ? 16 : 64;
        ham_base_palette = build_palette_rgb(parsed.cmap, base_size);
    }

    for (int y = 0; y < height; ++y) {
        // Decode row data
        if (compression_value == COMPRESSION_BYTERUN && !byterun_per_plane && byterun_per_scanline) {
            if (!unpack_byterun1(src, src_end, row_data.data(), bytes_per_row * stored_planes)) {
                return decode_result::failure(decode_error::truncated_data, "ByteRun1 decode failed");
            }
        } else {
            for (std::size_t p = 0; p < stored_planes; ++p) {
                std::uint8_t* dst = row_data.data() + p * bytes_per_row;
                if (compression_value == COMPRESSION_NONE) {
                    if (src + bytes_per_row > src_end) {
                        return decode_result::failure(decode_error::truncated_data, "Unexpected end of data");
                    }
                    std::memcpy(dst, src, bytes_per_row);
                    src += bytes_per_row;
                } else {
                    if (!unpack_byterun1(src, src_end, dst, bytes_per_row)) {
                        return decode_result::failure(decode_error::truncated_data, "ByteRun1 decode failed");
                    }
                }
            }
        }

        // Convert planar to chunky
        if (is_truecolor) {
            std::vector<std::uint8_t> rgba_row(static_cast<std::size_t>(width) * 4);
            for (int x = 0; x < width; ++x) {
                const std::size_t byte_index = static_cast<std::size_t>(x) / 8;
                const std::uint8_t bit_mask = static_cast<std::uint8_t>(0x80u >> (static_cast<std::size_t>(x) % 8));

                auto read_channel = [&](std::size_t base_plane) {
                    std::uint8_t value = 0;
                    for (std::size_t bit = 0; bit < 8; ++bit) {
                        const std::size_t plane = base_plane + bit;
                        const std::uint8_t byte = row_data[plane * bytes_per_row + byte_index];
                        value |= static_cast<std::uint8_t>(((byte & bit_mask) ? 1 : 0) << bit);
                    }
                    return value;
                };

                const std::uint8_t r = read_channel(0);
                const std::uint8_t g = read_channel(8);
                const std::uint8_t b = read_channel(16);
                std::uint8_t a = 0xFF;

                if (plane_count == 32) {
                    a = read_channel(24);
                }

                if (has_mask) {
                    const std::uint8_t mask_byte = row_data[plane_count * bytes_per_row + byte_index];
                    if ((mask_byte & bit_mask) == 0) {
                        a = 0;
                    }
                }

                rgba_row[static_cast<std::size_t>(x) * 4 + 0] = r;
                rgba_row[static_cast<std::size_t>(x) * 4 + 1] = g;
                rgba_row[static_cast<std::size_t>(x) * 4 + 2] = b;
                rgba_row[static_cast<std::size_t>(x) * 4 + 3] = a;
            }
            surf.write_pixels(0, y, width * 4, rgba_row.data());
        } else {
            // Extract indices from planar data
            for (int x = 0; x < width; ++x) {
                const std::size_t byte_index = static_cast<std::size_t>(x) / 8;
                const std::uint8_t bit_mask = static_cast<std::uint8_t>(0x80u >> (static_cast<std::size_t>(x) % 8));

                std::uint8_t index = 0;
                for (std::size_t p = 0; p < plane_count; ++p) {
                    const std::uint8_t byte = row_data[p * bytes_per_row + byte_index];
                    index |= static_cast<std::uint8_t>(((byte & bit_mask) ? 1 : 0) << p);
                }
                indices[static_cast<std::size_t>(x)] = index;
            }

            if (ham_mode && (plane_count == 6 || plane_count == 8)) {
                // HAM mode - decode to RGBA
                std::vector<std::uint8_t> rgba_row(static_cast<std::size_t>(width) * 4);
                const std::size_t data_bits = plane_count == 6 ? 4 : 6;

                auto expand = [&](std::uint8_t value) -> std::uint8_t {
                    if (data_bits == 4) {
                        return static_cast<std::uint8_t>((value << 4) | value);
                    }
                    if (data_bits == 6) {
                        return static_cast<std::uint8_t>(value << 2);
                    }
                    return value;
                };

                std::uint8_t r = 0, g = 0, b = 0;
                if (!ham_base_palette.empty()) {
                    r = ham_base_palette[0];
                    g = ham_base_palette[1];
                    b = ham_base_palette[2];
                }

                for (int x = 0; x < width; ++x) {
                    const std::uint8_t code = indices[static_cast<std::size_t>(x)];
                    const std::uint8_t op = static_cast<std::uint8_t>(code >> data_bits);
                    const std::uint8_t dat = static_cast<std::uint8_t>(code & ((1u << data_bits) - 1));

                    if (op == 0) {
                        const std::size_t pal_idx = static_cast<std::size_t>(dat) * 3;
                        if (pal_idx + 2 < ham_base_palette.size()) {
                            r = ham_base_palette[pal_idx + 0];
                            g = ham_base_palette[pal_idx + 1];
                            b = ham_base_palette[pal_idx + 2];
                        }
                    } else if (op == 1) {
                        b = expand(dat);
                    } else if (op == 2) {
                        r = expand(dat);
                    } else {
                        g = expand(dat);
                    }

                    rgba_row[static_cast<std::size_t>(x) * 4 + 0] = r;
                    rgba_row[static_cast<std::size_t>(x) * 4 + 1] = g;
                    rgba_row[static_cast<std::size_t>(x) * 4 + 2] = b;
                    rgba_row[static_cast<std::size_t>(x) * 4 + 3] = 0xFF;
                }
                surf.write_pixels(0, y, width * 4, rgba_row.data());
            } else {
                // Regular indexed mode
                surf.write_pixels(0, y, width, indices.data());
            }
        }
    }

    return decode_result::success();
}

} // namespace onyx_image
