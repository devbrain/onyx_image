#ifndef ONYX_IMAGE_CODECS_XPM_HPP_
#define ONYX_IMAGE_CODECS_XPM_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace onyx_image {

// ============================================================================
// XPM (X PixMap) Decoder
// ============================================================================

class ONYX_IMAGE_EXPORT xpm_decoder {
public:
    static constexpr std::string_view name = "xpm";
    static constexpr std::string_view extensions[] = {".xpm"};

    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;

    [[nodiscard]] static decode_result decode(std::span<const std::uint8_t> data,
                                               surface& surf,
                                               const decode_options& options = {});
};

} // namespace onyx_image

#endif // ONYX_IMAGE_CODECS_XPM_HPP_
