#ifndef ONYX_IMAGE_CODECS_INTERPAINT_HPP_
#define ONYX_IMAGE_CODECS_INTERPAINT_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace onyx_image {

// ============================================================================
// InterPaint Decoder (C64 graphics formats)
// ============================================================================
//
// InterPaint is a C64 graphics editor that produces two format variants:
//
// IPH (InterPaint Hires):
// - High-resolution mode: 320x200 at 1:1 aspect ratio
// - 2 colors per 8x8 character cell
// - File sizes: 9002, 9003, or 9009 bytes
//
// IPT (InterPaint multicolor):
// - Multicolor mode: 160x200 (displayed as 320x200 with 2:1 aspect)
// - 4 colors per 4x8 character cell
// - File size: 10003 bytes (same layout as Koala)
//
// Supported extensions: .iph, .ipt

class ONYX_IMAGE_EXPORT interpaint_decoder {
public:
    static constexpr std::string_view name = "interpaint";
    static constexpr std::string_view extensions[] = {".iph", ".ipt"};

    /**
     * Check if data appears to be an InterPaint file.
     * @param data Raw file data
     * @return true if the data appears to be InterPaint format
     */
    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;

    /**
     * Decode InterPaint image data to a surface.
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

#endif // ONYX_IMAGE_CODECS_INTERPAINT_HPP_
