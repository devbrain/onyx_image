#include <onyx_image/codec.hpp>

// Per-codec headers are guarded by ONYX_IMAGE_HAS_<NAME> compile defs
// set in src/CMakeLists.txt. Disabled codecs have their .cpp omitted
// from the build, so this TU must also omit the corresponding _impl
// class and registration to avoid undefined references at link.
#if defined(ONYX_IMAGE_HAS_PCX)
#include <onyx_image/codecs/pcx.hpp>
#endif
#if defined(ONYX_IMAGE_HAS_PNG)
#include <onyx_image/codecs/png.hpp>
#endif
#if defined(ONYX_IMAGE_HAS_LBM)
#include <onyx_image/codecs/lbm.hpp>
#endif
#if defined(ONYX_IMAGE_HAS_STB)
#include <onyx_image/codecs/jpeg.hpp>
#include <onyx_image/codecs/tga.hpp>
#include <onyx_image/codecs/gif.hpp>
#endif
#if defined(ONYX_IMAGE_HAS_WEBP)
#include <onyx_image/codecs/webp.hpp>
#endif
#if defined(ONYX_IMAGE_HAS_BMP)
#include <onyx_image/codecs/bmp.hpp>
#endif
#if defined(ONYX_IMAGE_HAS_SUNRAST)
#include <onyx_image/codecs/sunrast.hpp>
#endif
#if defined(ONYX_IMAGE_HAS_PICTOR)
#include <onyx_image/codecs/pictor.hpp>
#endif
#if defined(ONYX_IMAGE_HAS_SGI)
#include <onyx_image/codecs/sgi.hpp>
#endif
#if defined(ONYX_IMAGE_HAS_PNM)
#include <onyx_image/codecs/pnm.hpp>
#endif
#if defined(ONYX_IMAGE_HAS_DCX)
#include <onyx_image/codecs/dcx.hpp>
#endif
#if defined(ONYX_IMAGE_HAS_MSP)
#include <onyx_image/codecs/msp.hpp>
#endif
#if defined(ONYX_IMAGE_HAS_ATARIST)
#include <onyx_image/codecs/atarist.hpp>
#endif
#if defined(ONYX_IMAGE_HAS_QOI)
#include <onyx_image/codecs/qoi.hpp>
#endif
#if defined(ONYX_IMAGE_HAS_ICO)
#include <onyx_image/codecs/ico.hpp>
#endif
#if defined(ONYX_IMAGE_HAS_XPM)
#include <onyx_image/codecs/xpm.hpp>
#endif
#if defined(ONYX_IMAGE_HAS_KOALA)
#include <onyx_image/codecs/koala.hpp>
#endif
#if defined(ONYX_IMAGE_HAS_C64_DOODLE)
#include <onyx_image/codecs/c64_doodle.hpp>
#endif
#if defined(ONYX_IMAGE_HAS_DRAZLACE)
#include <onyx_image/codecs/drazlace.hpp>
#endif
#if defined(ONYX_IMAGE_HAS_INTERPAINT)
#include <onyx_image/codecs/interpaint.hpp>
#endif
#if defined(ONYX_IMAGE_HAS_AMI)
#include <onyx_image/codecs/ami.hpp>
#endif
#if defined(ONYX_IMAGE_HAS_FUNPAINT)
#include <onyx_image/codecs/funpaint.hpp>
#endif
#if defined(ONYX_IMAGE_HAS_C64_HIRES)
#include <onyx_image/codecs/c64_hires.hpp>
#endif
#if defined(ONYX_IMAGE_HAS_RUNPAINT)
#include <onyx_image/codecs/runpaint.hpp>
#endif

#include <algorithm>

namespace onyx_image {

// ============================================================================
// Decoder Wrappers
// ============================================================================

namespace {

#if defined(ONYX_IMAGE_HAS_PCX)
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
#endif

#if defined(ONYX_IMAGE_HAS_PNG)
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
#endif

#if defined(ONYX_IMAGE_HAS_LBM)
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
#endif

#if defined(ONYX_IMAGE_HAS_STB)
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
#endif

#if defined(ONYX_IMAGE_HAS_STB)
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
#endif

#if defined(ONYX_IMAGE_HAS_STB)
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
#endif

#if defined(ONYX_IMAGE_HAS_WEBP)
class webp_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return webp_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return webp_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return webp_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return webp_decoder::decode(data, surf, options);
    }
};
#endif

#if defined(ONYX_IMAGE_HAS_BMP)
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
#endif

#if defined(ONYX_IMAGE_HAS_SUNRAST)
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
#endif

#if defined(ONYX_IMAGE_HAS_PICTOR)
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
#endif

#if defined(ONYX_IMAGE_HAS_SGI)
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
#endif

#if defined(ONYX_IMAGE_HAS_PNM)
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
#endif

#if defined(ONYX_IMAGE_HAS_DCX)
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
#endif

#if defined(ONYX_IMAGE_HAS_MSP)
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
#endif

#if defined(ONYX_IMAGE_HAS_ATARIST)
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
#endif

#if defined(ONYX_IMAGE_HAS_ATARIST)
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
#endif

#if defined(ONYX_IMAGE_HAS_ATARIST)
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
#endif

#if defined(ONYX_IMAGE_HAS_ATARIST)
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
#endif

#if defined(ONYX_IMAGE_HAS_ATARIST)
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
#endif

#if defined(ONYX_IMAGE_HAS_ATARIST)
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
#endif

#if defined(ONYX_IMAGE_HAS_ATARIST)
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
#endif

#if defined(ONYX_IMAGE_HAS_QOI)
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
#endif

#if defined(ONYX_IMAGE_HAS_ICO)
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
#endif

#if defined(ONYX_IMAGE_HAS_ICO)
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
#endif

#if defined(ONYX_IMAGE_HAS_XPM)
class xpm_decoder_impl : public decoder {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return xpm_decoder::name;
    }

    [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
        return xpm_decoder::extensions;
    }

    [[nodiscard]] bool sniff(std::span<const std::uint8_t> data) const noexcept override {
        return xpm_decoder::sniff(data);
    }

    [[nodiscard]] decode_result decode(std::span<const std::uint8_t> data,
                                        surface& surf,
                                        const decode_options& options) const override {
        return xpm_decoder::decode(data, surf, options);
    }
};
#endif

#if defined(ONYX_IMAGE_HAS_KOALA)
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
#endif

#if defined(ONYX_IMAGE_HAS_C64_DOODLE)
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
#endif

#if defined(ONYX_IMAGE_HAS_DRAZLACE)
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
#endif

#if defined(ONYX_IMAGE_HAS_INTERPAINT)
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
#endif

#if defined(ONYX_IMAGE_HAS_AMI)
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
#endif

#if defined(ONYX_IMAGE_HAS_FUNPAINT)
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
#endif

#if defined(ONYX_IMAGE_HAS_C64_HIRES)
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
#endif

#if defined(ONYX_IMAGE_HAS_RUNPAINT)
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
#endif

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
#if defined(ONYX_IMAGE_HAS_PCX)
    decoders_.push_back(std::make_unique<pcx_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_PNG)
    decoders_.push_back(std::make_unique<png_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_LBM)
    decoders_.push_back(std::make_unique<lbm_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_STB)
    decoders_.push_back(std::make_unique<jpeg_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_STB)
    decoders_.push_back(std::make_unique<tga_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_STB)
    decoders_.push_back(std::make_unique<gif_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_WEBP)
    decoders_.push_back(std::make_unique<webp_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_BMP)
    decoders_.push_back(std::make_unique<bmp_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_SUNRAST)
    decoders_.push_back(std::make_unique<sunrast_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_PICTOR)
    decoders_.push_back(std::make_unique<pictor_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_SGI)
    decoders_.push_back(std::make_unique<sgi_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_PNM)
    decoders_.push_back(std::make_unique<pnm_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_DCX)
    decoders_.push_back(std::make_unique<dcx_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_MSP)
    decoders_.push_back(std::make_unique<msp_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_ATARIST)
    decoders_.push_back(std::make_unique<neo_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_ATARIST)
    decoders_.push_back(std::make_unique<degas_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_ATARIST)
    decoders_.push_back(std::make_unique<crack_art_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_ATARIST)
    decoders_.push_back(std::make_unique<spectrum512_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_ATARIST)
    decoders_.push_back(std::make_unique<photochrome_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_ATARIST)
    decoders_.push_back(std::make_unique<tiny_stuff_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_ATARIST)
    decoders_.push_back(std::make_unique<doodle_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_QOI)
    decoders_.push_back(std::make_unique<qoi_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_ICO)
    decoders_.push_back(std::make_unique<ico_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_ICO)
    decoders_.push_back(std::make_unique<exe_icon_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_XPM)
    decoders_.push_back(std::make_unique<xpm_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_C64_DOODLE)
    decoders_.push_back(std::make_unique<c64_doodle_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_RUNPAINT)
    decoders_.push_back(std::make_unique<runpaint_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_INTERPAINT)
    decoders_.push_back(std::make_unique<interpaint_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_AMI)
    decoders_.push_back(std::make_unique<ami_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_FUNPAINT)
    decoders_.push_back(std::make_unique<funpaint_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_C64_HIRES)
    decoders_.push_back(std::make_unique<c64_hires_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_KOALA)
    decoders_.push_back(std::make_unique<koala_decoder_impl>());
#endif
#if defined(ONYX_IMAGE_HAS_DRAZLACE)
    decoders_.push_back(std::make_unique<drazlace_decoder_impl>());
#endif
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
