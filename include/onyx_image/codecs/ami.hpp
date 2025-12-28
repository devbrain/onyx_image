#ifndef ONYX_IMAGE_CODECS_AMI_HPP_
#define ONYX_IMAGE_CODECS_AMI_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace onyx_image {

// ============================================================================
// AMI Decoder (Amica Paint C64 format)
// ============================================================================
//
// AMI is a C64 image format created by Amica Paint, a German paint program.
// It stores multicolor images using DRP (DrazPaint) RLE compression.
//
// Format:
// - 2-byte load address (typically 0x4000)
// - DRP RLE compressed data with fixed escape byte 0xc2
// - Decompresses to 10001 bytes (standard Koala layout)
//
// Koala layout (after decompression):
// - Offset 0x0000: Bitmap data (8000 bytes)
// - Offset 0x1f40: Screen RAM (1000 bytes)
// - Offset 0x2328: Color RAM (1000 bytes)
// - Offset 0x2710: Background color (1 byte)
//
// Output: 320x200 RGB image (C64 multicolor mode, 2:1 aspect ratio)
//
// Supported extension: .ami

class ONYX_IMAGE_EXPORT ami_decoder {
public:
    static constexpr std::string_view name = "ami";
    static constexpr std::string_view extensions[] = {".ami"};

    /**
     * Check if data appears to be an AMI file.
     * @param data Raw file data
     * @return true if the data appears to be AMI format
     */
    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;

    /**
     * Decode AMI image data to a surface.
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

#endif // ONYX_IMAGE_CODECS_AMI_HPP_
