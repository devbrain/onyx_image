#ifndef ONYX_IMAGE_CODECS_BMP_HPP_
#define ONYX_IMAGE_CODECS_BMP_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace onyx_image {

// ============================================================================
// BMP/DIB Decoder
// ============================================================================

class ONYX_IMAGE_EXPORT bmp_decoder {
public:
    static constexpr std::string_view name = "bmp";
    static constexpr std::string_view extensions[] = {".bmp", ".dib"};

    /**
     * Check if data appears to be a BMP file.
     * @param data Raw file data
     * @return true if the signature matches BMP format
     */
    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;

    /**
     * Decode BMP image data to a surface.
     * Supports:
     *   - Windows BMP (BITMAPINFOHEADER and later)
     *   - OS/2 BMP (BITMAPCOREHEADER and OS/2 2.x)
     *   - 1, 2, 4, 8, 16, 24, and 32-bit color depths
     *   - RLE4 and RLE8 compression
     *   - BI_BITFIELDS for 16-bit and 32-bit images
     *   - Top-down and bottom-up images
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

#endif // ONYX_IMAGE_CODECS_BMP_HPP_
