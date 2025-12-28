#ifndef ONYX_IMAGE_CODECS_SUNRAST_HPP_
#define ONYX_IMAGE_CODECS_SUNRAST_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace onyx_image {

// ============================================================================
// Sun Raster Decoder
// ============================================================================

class ONYX_IMAGE_EXPORT sunrast_decoder {
public:
    static constexpr std::string_view name = "sunrast";
    static constexpr std::string_view extensions[] = {".ras", ".sun"};

    /**
     * Check if data appears to be a Sun Raster file.
     * @param data Raw file data
     * @return true if the signature matches Sun Raster format
     */
    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;

    /**
     * Decode Sun Raster image data to a surface.
     * Supports:
     *   - 1, 8, 24, and 32-bit color depths
     *   - Uncompressed (standard) and RLE-compressed formats
     *   - RGB and BGR pixel ordering
     *   - RGB colormaps for 8-bit images
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

#endif // ONYX_IMAGE_CODECS_SUNRAST_HPP_
