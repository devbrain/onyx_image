#ifndef ONYX_IMAGE_CODECS_FUNPAINT_HPP_
#define ONYX_IMAGE_CODECS_FUNPAINT_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace onyx_image {

// ============================================================================
// FunPaint Decoder (C64 IFLI graphics format)
// ============================================================================
//
// FunPaint is a C64 graphics editor that produces IFLI (Interlaced FLI) images.
// FLI (Flexible Line Interpreter) is a technique that allows more colors per
// character cell by changing video registers during each scanline.
// IFLI combines two FLI frames with interlacing for even more colors.
//
// Format:
// - 2-byte load address (0x3f00)
// - Signature: "FUNPAINT (MT) " (14 bytes at offset 2)
// - Compression flag at offset 16 (0 = uncompressed, non-zero = compressed)
// - Escape byte at offset 17 (for DRP RLE compression)
// - Data starts at offset 18 (compressed) or 2 (uncompressed)
// - Uncompressed size: 33694 bytes
//
// IFLI layout (after decompression):
// - Two bitmap frames (8000 bytes each)
// - Two video matrix banks (8x1000 bytes each for 8 scanline groups)
// - One shared color RAM (1000 bytes)
//
// Output: 296x200 RGB image (due to FLI bug, first 24 pixels are unusable)
//
// Supported extensions: .fp2, .fun, .vic

class ONYX_IMAGE_EXPORT funpaint_decoder {
public:
    static constexpr std::string_view name = "funpaint";
    static constexpr std::string_view extensions[] = {".fp2", ".fun", ".vic"};

    /**
     * Check if data appears to be a FunPaint file.
     * @param data Raw file data
     * @return true if the data appears to be FunPaint format
     */
    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;

    /**
     * Decode FunPaint image data to a surface.
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

#endif // ONYX_IMAGE_CODECS_FUNPAINT_HPP_
