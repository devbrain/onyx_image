#ifndef ONYX_IMAGE_CODECS_PNG_HPP_
#define ONYX_IMAGE_CODECS_PNG_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

namespace onyx_image {

// ============================================================================
// PNG Decoder
// ============================================================================

class ONYX_IMAGE_EXPORT png_decoder {
public:
    static constexpr std::string_view name = "png";
    static constexpr std::string_view extensions[] = {".png"};

    /**
     * Check if data appears to be a PNG file.
     * @param data Raw file data
     * @return true if the signature matches PNG format
     */
    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;

    /**
     * Decode PNG image data to a surface.
     * @param data Raw file data
     * @param surf Destination surface
     * @param options Decode options
     * @return Decode result with success/error status
     */
    [[nodiscard]] static decode_result decode(std::span<const std::uint8_t> data,
                                               surface& surf,
                                               const decode_options& options = {});
};

// ============================================================================
// PNG Encoder Functions
// ============================================================================

/**
 * Encode a memory surface to PNG format.
 * @param surf Source surface
 * @return PNG-encoded data, or empty vector on failure
 */
[[nodiscard]] ONYX_IMAGE_EXPORT std::vector<std::uint8_t> encode_png(const memory_surface& surf);

/**
 * Save a memory surface to a PNG file.
 * @param surf Source surface
 * @param path Output file path
 * @return true on success
 */
[[nodiscard]] ONYX_IMAGE_EXPORT bool save_png(const memory_surface& surf,
                                               const std::filesystem::path& path);

// ============================================================================
// PNG Surface
// ============================================================================

/**
 * Surface that can save its contents as PNG.
 * Inherits from memory_surface and adds save functionality.
 */
class ONYX_IMAGE_EXPORT png_surface : public memory_surface {
public:
    png_surface() = default;
    ~png_surface() override = default;

    png_surface(const png_surface&) = delete;
    png_surface& operator=(const png_surface&) = delete;
    png_surface(png_surface&&) noexcept = default;
    png_surface& operator=(png_surface&&) noexcept = default;

    /**
     * Encode surface contents to PNG format.
     * @return PNG-encoded data, or empty vector on failure
     */
    [[nodiscard]] std::vector<std::uint8_t> encode() const;

    /**
     * Save surface contents to a PNG file.
     * @param path Output file path
     * @return true on success
     */
    [[nodiscard]] bool save(const std::filesystem::path& path) const;
};

} // namespace onyx_image

#endif // ONYX_IMAGE_CODECS_PNG_HPP_
