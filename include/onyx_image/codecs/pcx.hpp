#ifndef ONYX_IMAGE_CODECS_PCX_HPP_
#define ONYX_IMAGE_CODECS_PCX_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace onyx_image {

// ============================================================================
// PCX Decoder
// ============================================================================

class ONYX_IMAGE_EXPORT pcx_decoder {
public:
    static constexpr std::string_view name = "pcx";
    static constexpr std::string_view extensions[] = {".pcx", ".pcc"};

    /**
     * Check if data appears to be a PCX file.
     * @param data Raw file data
     * @return true if the signature matches PCX format
     */
    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;

    /**
     * Decode PCX image data to a surface.
     * @param data Raw file data
     * @param surf Destination surface
     * @param options Decode options
     * @return Decode result with success/error status
     */
    [[nodiscard]] static decode_result decode(std::span<const std::uint8_t> data,
                                               surface& surf,
                                               const decode_options& options = {});

    // Header info structure (public for DCX multi-page support)
    struct header_info {
        int width;
        int height;
        int bits_per_pixel;
        int num_planes;
        int bytes_per_line;
        int version;
        bool has_rle;
    };

    // Parse PCX header without decoding (public for DCX multi-page support)
    [[nodiscard]] static decode_result parse_header(std::span<const std::uint8_t> data,
                                                     header_info& info,
                                                     const decode_options& options);

private:

    [[nodiscard]] static decode_result decode_rle(std::span<const std::uint8_t> data,
                                                   std::size_t data_offset,
                                                   const header_info& info,
                                                   surface& surf);

    static void apply_ega_palette(std::span<const std::uint8_t> header_data,
                                   surface& surf);

    static void apply_cga_palette(std::span<const std::uint8_t> header_data,
                                   surface& surf);

    [[nodiscard]] static decode_result apply_vga_palette(std::span<const std::uint8_t> data,
                                                          surface& surf);
};

} // namespace onyx_image

#endif // ONYX_IMAGE_CODECS_PCX_HPP_
