#ifndef ONYX_IMAGE_CODECS_DRAZLACE_HPP_
#define ONYX_IMAGE_CODECS_DRAZLACE_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace onyx_image {

// ============================================================================
// DrazLace Decoder (C64 interlaced multicolor graphics format)
// ============================================================================
//
// DrazLace is a C64 interlaced multicolor graphics format created by
// the DrazPaint editor. It stores two multicolor bitmap frames that are
// blended together to simulate higher color depth through interlacing.
//
// Format details:
// - Resolution: 320x200 (160x200 multicolor pixels, doubled horizontally)
// - Two frames blended for interlace effect
// - Can be compressed (RLE) or uncompressed
// - Uncompressed size: 18242 bytes
// - Signature: "DRAZLACE! 1.0" at offset 2 (compressed files)
//
// Supported extensions: .drl

class ONYX_IMAGE_EXPORT drazlace_decoder {
public:
    static constexpr std::string_view name = "drazlace";
    static constexpr std::string_view extensions[] = {".drl"};

    /**
     * Check if data appears to be a DrazLace file.
     * @param data Raw file data
     * @return true if the data appears to be DrazLace format
     */
    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;

    /**
     * Decode DrazLace image data to a surface.
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

#endif // ONYX_IMAGE_CODECS_DRAZLACE_HPP_
