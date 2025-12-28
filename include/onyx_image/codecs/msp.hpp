#ifndef ONYX_IMAGE_CODECS_MSP_HPP_
#define ONYX_IMAGE_CODECS_MSP_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace onyx_image {

// ============================================================================
// MSP (Microsoft Paint) Decoder
// ============================================================================

class ONYX_IMAGE_EXPORT msp_decoder {
public:
    static constexpr std::string_view name = "msp";
    static constexpr std::string_view extensions[] = {".msp"};

    /**
     * Check if data appears to be an MSP file.
     * @param data Raw file data
     * @return true if the signature matches MSP format (v1 or v2)
     */
    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;

    /**
     * Decode MSP image data to a surface.
     * Supports both version 1 (uncompressed) and version 2 (RLE compressed).
     * Output is 1-bit monochrome converted to indexed8 with 2-entry palette.
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

#endif // ONYX_IMAGE_CODECS_MSP_HPP_
