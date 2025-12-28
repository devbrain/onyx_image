#ifndef ONYX_IMAGE_CODECS_C64_DOODLE_HPP_
#define ONYX_IMAGE_CODECS_C64_DOODLE_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace onyx_image {

// ============================================================================
// C64 Doodle Decoder (C64 high-resolution graphics format)
// ============================================================================
//
// Doodle is a high-resolution bitmap graphics format for the Commodore 64.
// Unlike multicolor mode, it provides full 320x200 resolution with 2 colors
// per 8x8 character cell.
//
// Supported formats:
// - .dd, .ddp, .jj: Doodle format files
// - Run Paint, Hires-Editor variants (by file size)
// - JJ: RLE compressed Doodle

class ONYX_IMAGE_EXPORT c64_doodle_decoder {
public:
    static constexpr std::string_view name = "c64_doodle";
    static constexpr std::string_view extensions[] = {".dd", ".ddp", ".jj"};

    /**
     * Check if data appears to be a C64 Doodle file.
     * @param data Raw file data
     * @return true if the data appears to be Doodle format
     */
    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;

    /**
     * Decode C64 Doodle image data to a surface.
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

#endif // ONYX_IMAGE_CODECS_C64_DOODLE_HPP_
