#ifndef ONYX_IMAGE_CODECS_C64_HIRES_HPP_
#define ONYX_IMAGE_CODECS_C64_HIRES_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace onyx_image {

// ============================================================================
// C64 Hires Decoder (C64 high-resolution bitmap graphics)
// ============================================================================
//
// C64 hires mode provides 320x200 resolution with 2 colors per 8x8 character
// cell. Unlike multicolor mode, each pixel is represented by a single bit.
//
// Supported formats:
//
// 8002 bytes (bitmap only, fixed colors):
// - 2-byte load address
// - 8000 bytes bitmap data
// - Extensions: .hbm, .fgs, .gih, .rpo, .dd, .mon, .gcd
// - Colors determined by format variant:
//   - HBM/FGS style: black background, white foreground
//   - RPO/GIH style: white background, black foreground
//
// 9002/9003/9009 bytes (bitmap + video matrix):
// - 2-byte load address
// - 8000 bytes bitmap data
// - 1000 bytes video matrix (colors per character cell)
// - Extensions: .hpi (when this size), .aas, .art
//
// Output: 320x200 RGB image

class ONYX_IMAGE_EXPORT c64_hires_decoder {
public:
    static constexpr std::string_view name = "c64_hires";
    static constexpr std::string_view extensions[] = {
        ".hbm", ".fgs", ".gih", ".rpo", ".dd", ".mon", ".gcd", ".hpi"
    };

    /**
     * Check if data appears to be a C64 hires file.
     * @param data Raw file data
     * @return true if the data appears to be C64 hires format
     */
    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;

    /**
     * Decode C64 hires image data to a surface.
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

#endif // ONYX_IMAGE_CODECS_C64_HIRES_HPP_
