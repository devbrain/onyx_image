#ifndef ONYX_IMAGE_CODECS_SGI_HPP_
#define ONYX_IMAGE_CODECS_SGI_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace onyx_image {

// ============================================================================
// SGI (Silicon Graphics Image) Decoder
// ============================================================================

class ONYX_IMAGE_EXPORT sgi_decoder {
public:
    static constexpr std::string_view name = "sgi";
    static constexpr std::string_view extensions[] = {".sgi", ".rgb", ".rgba", ".bw"};

    /**
     * Check if data appears to be an SGI image file.
     * @param data Raw file data
     * @return true if the signature matches SGI format
     */
    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;

    /**
     * Decode SGI image data to a surface.
     * Supports:
     *   - Grayscale (1 channel)
     *   - RGB (3 channels)
     *   - RGBA (4 channels)
     *   - Uncompressed and RLE compression
     *   - 8-bit and 16-bit per channel
     *
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

#endif // ONYX_IMAGE_CODECS_SGI_HPP_
