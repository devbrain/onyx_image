#include <onyx_image/codec.hpp>
#include <onyx_image/codecs/pcx.hpp>
#include <onyx_image/codecs/png.hpp>
#include <onyx_image/codecs/lbm.hpp>
#include <onyx_image/codecs/jpeg.hpp>
#include <onyx_image/codecs/tga.hpp>
#include <onyx_image/codecs/gif.hpp>
#include <onyx_image/codecs/bmp.hpp>
#include <onyx_image/codecs/sunrast.hpp>
#include <onyx_image/codecs/pictor.hpp>
#include <onyx_image/codecs/sgi.hpp>
#include <onyx_image/codecs/pnm.hpp>
#include <onyx_image/codecs/dcx.hpp>
#include <onyx_image/codecs/msp.hpp>
#include <onyx_image/codecs/atarist.hpp>
#include <onyx_image/codecs/qoi.hpp>
#include <onyx_image/codecs/ico.hpp>
#include <onyx_image/codecs/koala.hpp>
#include <onyx_image/codecs/c64_doodle.hpp>
#include <onyx_image/codecs/drazlace.hpp>
#include <onyx_image/codecs/interpaint.hpp>
#include <onyx_image/codecs/ami.hpp>
#include <onyx_image/codecs/funpaint.hpp>
#include <onyx_image/codecs/c64_hires.hpp>
#include <onyx_image/codecs/runpaint.hpp>

#include <algorithm>

namespace onyx_image {

// ============================================================================
// Decoder Wrappers
// ============================================================================

namespace {

class pcx_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return pcx_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return pcx_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return pcx_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return pcx_decoder::decode(data, surf, options);
    }
};

class png_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return png_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return png_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return png_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return png_decoder::decode(data, surf, options);
    }
};

class lbm_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return lbm_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return lbm_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return lbm_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return lbm_decoder::decode(data, surf, options);
    }
};

class jpeg_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return jpeg_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return jpeg_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return jpeg_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return jpeg_decoder::decode(data, surf, options);
    }
};

class tga_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return tga_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return tga_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return tga_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return tga_decoder::decode(data, surf, options);
    }
};

class gif_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return gif_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return gif_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return gif_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return gif_decoder::decode(data, surf, options);
    }
};

class bmp_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return bmp_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return bmp_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return bmp_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return bmp_decoder::decode(data, surf, options);
    }
};

class sunrast_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return sunrast_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return sunrast_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return sunrast_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return sunrast_decoder::decode(data, surf, options);
    }
};

class pictor_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return pictor_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return pictor_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return pictor_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return pictor_decoder::decode(data, surf, options);
    }
};

class sgi_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return sgi_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return sgi_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return sgi_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return sgi_decoder::decode(data, surf, options);
    }
};

class pnm_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return pnm_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return pnm_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return pnm_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return pnm_decoder::decode(data, surf, options);
    }
};

class dcx_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return dcx_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return dcx_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return dcx_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return dcx_decoder::decode(data, surf, options);
    }
};

class msp_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return msp_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return msp_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return msp_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return msp_decoder::decode(data, surf, options);
    }
};

class neo_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return neo_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return neo_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return neo_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return neo_decoder::decode(data, surf, options);
    }
};

class degas_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return degas_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return degas_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return degas_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return degas_decoder::decode(data, surf, options);
    }
};

class doodle_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return doodle_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return doodle_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return doodle_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return doodle_decoder::decode(data, surf, options);
    }
};

class crack_art_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return crack_art_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return crack_art_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return crack_art_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return crack_art_decoder::decode(data, surf, options);
    }
};

class tiny_stuff_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return tiny_stuff_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return tiny_stuff_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return tiny_stuff_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return tiny_stuff_decoder::decode(data, surf, options);
    }
};

class spectrum512_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return spectrum512_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return spectrum512_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return spectrum512_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return spectrum512_decoder::decode(data, surf, options);
    }
};

class photochrome_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return photochrome_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return photochrome_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return photochrome_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return photochrome_decoder::decode(data, surf, options);
    }
};

class qoi_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return qoi_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return qoi_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return qoi_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return qoi_decoder::decode(data, surf, options);
    }
};

class ico_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return ico_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return ico_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return ico_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return ico_decoder::decode(data, surf, options);
    }
};

class exe_icon_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return exe_icon_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return exe_icon_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return exe_icon_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return exe_icon_decoder::decode(data, surf, options);
    }
};

class koala_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return koala_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return koala_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return koala_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return koala_decoder::decode(data, surf, options);
    }
};

class c64_doodle_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return c64_doodle_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return c64_doodle_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return c64_doodle_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return c64_doodle_decoder::decode(data, surf, options);
    }
};

class drazlace_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return drazlace_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return drazlace_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return drazlace_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return drazlace_decoder::decode(data, surf, options);
    }
};

class interpaint_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return interpaint_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return interpaint_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return interpaint_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return interpaint_decoder::decode(data, surf, options);
    }
};

class ami_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return ami_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return ami_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return ami_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return ami_decoder::decode(data, surf, options);
    }
};

class funpaint_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return funpaint_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return funpaint_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return funpaint_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return funpaint_decoder::decode(data, surf, options);
    }
};

class c64_hires_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return c64_hires_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return c64_hires_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return c64_hires_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return c64_hires_decoder::decode(data, surf, options);
    }
};

class runpaint_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return runpaint_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return runpaint_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return runpaint_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return runpaint_decoder::decode(data, surf, options);
    }
};

} // namespace

// ============================================================================
// Codec Registry Implementation
// ============================================================================

codec_registry& codec_registry::instance() {
    static codec_registry registry;
    return registry;
}

codec_registry::codec_registry() {
    register_builtin_codecs();
}

codec_registry::~codec_registry() = default;

void codec_registry::register_builtin_codecs() {
    decoders_.push_back(std::make_unique<pcx_decoder_impl>());
    decoders_.push_back(std::make_unique<png_decoder_impl>());
    decoders_.push_back(std::make_unique<lbm_decoder_impl>());
    decoders_.push_back(std::make_unique<jpeg_decoder_impl>());
    decoders_.push_back(std::make_unique<tga_decoder_impl>());
    decoders_.push_back(std::make_unique<gif_decoder_impl>());
    decoders_.push_back(std::make_unique<bmp_decoder_impl>());
    decoders_.push_back(std::make_unique<sunrast_decoder_impl>());
    decoders_.push_back(std::make_unique<pictor_decoder_impl>());
    decoders_.push_back(std::make_unique<sgi_decoder_impl>());
    decoders_.push_back(std::make_unique<pnm_decoder_impl>());
    decoders_.push_back(std::make_unique<dcx_decoder_impl>());
    decoders_.push_back(std::make_unique<msp_decoder_impl>());
    decoders_.push_back(std::make_unique<neo_decoder_impl>());
    decoders_.push_back(std::make_unique<degas_decoder_impl>());
    decoders_.push_back(std::make_unique<crack_art_decoder_impl>());
    decoders_.push_back(std::make_unique<spectrum512_decoder_impl>());
    decoders_.push_back(std::make_unique<photochrome_decoder_impl>());
    decoders_.push_back(std::make_unique<tiny_stuff_decoder_impl>());
    decoders_.push_back(std::make_unique<doodle_decoder_impl>());
    decoders_.push_back(std::make_unique<qoi_decoder_impl>());
    decoders_.push_back(std::make_unique<ico_decoder_impl>());
    decoders_.push_back(std::make_unique<exe_icon_decoder_impl>());
    decoders_.push_back(std::make_unique<c64_doodle_decoder_impl>());
    decoders_.push_back(std::make_unique<runpaint_decoder_impl>());
    decoders_.push_back(std::make_unique<interpaint_decoder_impl>());
    decoders_.push_back(std::make_unique<ami_decoder_impl>());
    decoders_.push_back(std::make_unique<funpaint_decoder_impl>());
    decoders_.push_back(std::make_unique<c64_hires_decoder_impl>());
    decoders_.push_back(std::make_unique<koala_decoder_impl>());
    decoders_.push_back(std::make_unique<drazlace_decoder_impl>());
}

void codec_registry::register_decoder(std::unique_ptr<decoder> dec) {
    if (dec) {
        decoders_.push_back(std::move(dec));
    }
}

const decoder* codec_registry::find_decoder(std::span<const std::uint8_t> data) const {
    for (const auto& dec : decoders_) {
        if (dec->sniff(data)) {
            return dec.get();
        }
    }
    return nullptr;
}

const decoder* codec_registry::find_decoder(std::string_view name) const {
    for (const auto& dec : decoders_) {
        if (dec->name() == name) {
            return dec.get();
        }
    }
    return nullptr;
}

// ============================================================================
// Convenience Functions
// ============================================================================

decode_result decode(std::span<const std::uint8_t> data,
                     surface& surf,
                     const decode_options& options) {
    const auto* dec = codec_registry::instance().find_decoder(data);
    if (!dec) {
        return decode_result::failure(decode_error::invalid_format, "Unknown image format");
    }
    return dec->decode(data, surf, options);
}

decode_result decode(std::span<const std::uint8_t> data,
                     surface& surf,
                     std::string_view codec_name,
                     const decode_options& options) {
    const auto* dec = codec_registry::instance().find_decoder(codec_name);
    if (!dec) {
        return decode_result::failure(decode_error::invalid_format,
            std::string("Unknown codec: ") + std::string(codec_name));
    }
    return dec->decode(data, surf, options);
}

} // namespace onyx_image
