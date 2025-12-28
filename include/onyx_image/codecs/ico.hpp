#ifndef ONYX_IMAGE_CODECS_ICO_HPP_
#define ONYX_IMAGE_CODECS_ICO_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace onyx_image {

// ============================================================================
// ICO/CUR Decoder
// ============================================================================

/**
 * Decoder for Windows ICO (icon) and CUR (cursor) files.
 *
 * Supports:
 * - Standalone .ICO and .CUR files (multiple images)
 * - Icons embedded in Windows executables (NE, PE)
 * - 1, 4, 8, 24, and 32-bit color depths
 * - PNG-compressed icons (Vista+ format)
 * - Creates atlas with subrects for multi-image files
 *
 * Output:
 * - Single icon: decoded to surface at native size
 * - Multiple icons: vertically stacked atlas with subrects
 *   - subrect.kind = subrect_kind::sprite
 *   - subrect.user_tag = icon index
 */
class ONYX_IMAGE_EXPORT ico_decoder {
public:
    static constexpr std::string_view name = "ico";
    static constexpr std::string_view extensions[] = {".ico", ".cur"};

    /**
     * Check if data appears to be an ICO/CUR file.
     * @param data Raw file data
     * @return true if the signature matches ICO/CUR format
     */
    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;

    /**
     * Decode ICO/CUR file to a surface.
     *
     * For files with multiple icons, creates a vertical atlas with all
     * icons stacked. Each icon is accessible via subrects.
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

// ============================================================================
// EXE Icon Extractor
// ============================================================================

/**
 * Decoder for extracting icons from Windows/OS2 executables.
 *
 * Supports:
 * - NE (16-bit Windows) executables
 * - PE (32/64-bit Windows) executables
 * - LX (OS/2 2.x) executables
 * - Multiple icon resources per executable
 * - Creates atlas with all icons
 *
 * Uses libexe (mz-explode) for executable parsing.
 */
class ONYX_IMAGE_EXPORT exe_icon_decoder {
public:
    static constexpr std::string_view name = "exe_icon";
    static constexpr std::string_view extensions[] = {".exe", ".dll", ".scr"};

    /**
     * Check if data appears to be a Windows executable with icons.
     * @param data Raw file data
     * @return true if this is an NE/PE executable
     */
    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;

    /**
     * Extract icons from a Windows executable.
     *
     * Extracts all icons from the first icon group in the executable
     * and creates a vertical atlas.
     *
     * @param data Raw executable data
     * @param surf Destination surface
     * @param options Decode options
     * @return Decode result with success/error status
     */
    [[nodiscard]] static decode_result decode(std::span<const std::uint8_t> data,
                                               surface& surf,
                                               const decode_options& options = {});
};

} // namespace onyx_image

#endif // ONYX_IMAGE_CODECS_ICO_HPP_
