#ifndef ONYX_IMAGE_CODECS_DCX_HPP_
#define ONYX_IMAGE_CODECS_DCX_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace onyx_image {

// ============================================================================
// DCX (Multi-page PCX) Decoder
// ============================================================================

class ONYX_IMAGE_EXPORT dcx_decoder {
public:
    static constexpr std::string_view name = "dcx";
    static constexpr std::string_view extensions[] = {".dcx"};

    /**
     * Check if data appears to be a DCX file.
     * @param data Raw file data
     * @return true if the signature matches DCX format
     */
    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;

    /**
     * Decode DCX image data to a surface.
     * Decodes the first page of the multi-page DCX file.
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

#endif // ONYX_IMAGE_CODECS_DCX_HPP_
