#ifndef ONYX_IMAGE_CODECS_LBM_HPP_
#define ONYX_IMAGE_CODECS_LBM_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace onyx_image {

// ============================================================================
// LBM/ILBM Decoder
// ============================================================================

class ONYX_IMAGE_EXPORT lbm_decoder {
public:
    static constexpr std::string_view name = "lbm";
    static constexpr std::string_view extensions[] = {".lbm", ".ilbm", ".iff", ".bbm"};

    /**
     * Check if data appears to be an IFF ILBM/PBM file.
     * @param data Raw file data
     * @return true if the signature matches LBM format
     */
    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;

    /**
     * Decode LBM/ILBM image data to a surface.
     * Supports:
     *   - ILBM (planar) and PBM (chunky) formats
     *   - ByteRun1 compression
     *   - HAM (Hold-And-Modify) 6 and 8 modes
     *   - EHB (Extra Half-Brite) mode
     *   - Masking and transparency
     *   - 24/32-bit truecolor
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

#endif // ONYX_IMAGE_CODECS_LBM_HPP_
