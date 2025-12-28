#ifndef ONYX_IMAGE_CODECS_TGA_HPP_
#define ONYX_IMAGE_CODECS_TGA_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace onyx_image {

class ONYX_IMAGE_EXPORT tga_decoder {
public:
    static constexpr std::string_view name = "tga";
    static constexpr std::string_view extensions[] = {".tga", ".targa"};

    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;

    [[nodiscard]] static decode_result decode(std::span<const std::uint8_t> data,
                                               surface& surf,
                                               const decode_options& options = {});
};

} // namespace onyx_image

#endif // ONYX_IMAGE_CODECS_TGA_HPP_
