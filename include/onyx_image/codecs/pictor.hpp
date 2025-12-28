#ifndef ONYX_IMAGE_CODECS_PICTOR_HPP_
#define ONYX_IMAGE_CODECS_PICTOR_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace onyx_image {

// ============================================================================
// PICTOR/PC Paint Decoder
// ============================================================================

class ONYX_IMAGE_EXPORT pictor_decoder {
public:
    static constexpr std::string_view name = "pictor";
    static constexpr std::string_view extensions[] = {".pic", ".clp"};

    /**
     * Check if data appears to be a PICTOR file.
     * @param data Raw file data
     * @return true if the signature matches PICTOR format
     */
    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;

    /**
     * Decode PICTOR image data to a surface.
     * Supports:
     *   - CGA 2/4-color modes
     *   - EGA 16-color mode (planar)
     *   - VGA 256-color mode
     *   - RLE compression
     *   - Various palette types
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

#endif // ONYX_IMAGE_CODECS_PICTOR_HPP_
