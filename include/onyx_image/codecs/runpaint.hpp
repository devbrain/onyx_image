#ifndef ONYX_IMAGE_CODECS_RUNPAINT_HPP_
#define ONYX_IMAGE_CODECS_RUNPAINT_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace onyx_image {

// ============================================================================
// Run Paint Decoder (C64 multicolor graphics format)
// ============================================================================
//
// Run Paint is a C64 multicolor graphics editor format, essentially identical
// to Koala/InterPaint Tool format with a 2-byte load address prefix.
//
// File structure (10003 or 10006 bytes):
// - 2 bytes: Load address (typically $6000)
// - 8000 bytes: Bitmap data
// - 1000 bytes: Video matrix (screen RAM)
// - 1000 bytes: Color RAM
// - 1 byte: Background color
// - (optional 3 bytes padding for 10006 variant)
//
// Output: 320x200 RGB image (C64 multicolor, 160x200 effective resolution)

class ONYX_IMAGE_EXPORT runpaint_decoder {
public:
    static constexpr std::string_view name = "runpaint";
    static constexpr std::string_view extensions[] = {".rpm"};

    /**
     * Check if data appears to be a Run Paint file.
     * @param data Raw file data
     * @return true if the data appears to be Run Paint format
     */
    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;

    /**
     * Decode Run Paint image data to a surface.
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

#endif // ONYX_IMAGE_CODECS_RUNPAINT_HPP_
