#ifndef ONYX_IMAGE_CODECS_KOALA_HPP_
#define ONYX_IMAGE_CODECS_KOALA_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace onyx_image {

// ============================================================================
// Koala Decoder (C64 multicolor graphics format)
// ============================================================================
//
// Koala is a bitmap graphics format for the Commodore 64, representing
// multicolor graphics mode with 320x200 pixels at 2:1 aspect ratio.
//
// Supported formats:
// - .koa, .kla, .koala: Standard Koala format (10001 or 10003 bytes)
// - .gg: GodotGames compressed Koala (RLE compression)
// - .gig: Same as .koa

class ONYX_IMAGE_EXPORT koala_decoder {
public:
    static constexpr std::string_view name = "koala";
    static constexpr std::string_view extensions[] = {".koa", ".kla", ".koala", ".gg", ".gig"};

    /**
     * Check if data appears to be a Koala file.
     * @param data Raw file data
     * @return true if the data appears to be Koala format
     */
    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;

    /**
     * Decode Koala image data to a surface.
     * @param data Raw file data
     * @param surf Destination surface
     * @param options Decode options
     * @return Decode result with success/error status
     */
    [[nodiscard]] static decode_result decode(std::span<const std::uint8_t> data,
                                               surface& surf,
                                               const decode_options& options = {});
};

} // namespace onyx_image

#endif // ONYX_IMAGE_CODECS_KOALA_HPP_
