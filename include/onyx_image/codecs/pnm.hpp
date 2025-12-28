#ifndef ONYX_IMAGE_CODECS_PNM_HPP_
#define ONYX_IMAGE_CODECS_PNM_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace onyx_image {

// ============================================================================
// PNM (Portable aNyMap) Decoder - PPM, PGM, PBM formats
// ============================================================================

class ONYX_IMAGE_EXPORT pnm_decoder {
public:
    static constexpr std::string_view name = "pnm";
    static constexpr std::string_view extensions[] = {".ppm", ".pgm", ".pbm", ".pnm"};

    /**
     * Check if data appears to be a PNM image file.
     * @param data Raw file data
     * @return true if the signature matches PNM format (P1-P6)
     */
    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;

    /**
     * Decode PNM image data to a surface.
     * Supports:
     *   - P1: ASCII PBM (bitmap)
     *   - P2: ASCII PGM (grayscale)
     *   - P3: ASCII PPM (RGB)
     *   - P4: Binary PBM (bitmap)
     *   - P5: Binary PGM (grayscale, 8/16-bit)
     *   - P6: Binary PPM (RGB, 8/16-bit)
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

#endif // ONYX_IMAGE_CODECS_PNM_HPP_
