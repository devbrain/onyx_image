#ifndef ONYX_IMAGE_CODECS_ATARIST_HPP_
#define ONYX_IMAGE_CODECS_ATARIST_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace onyx_image {

// ============================================================================
// NEO (Neochrome) Decoder
// ============================================================================

class ONYX_IMAGE_EXPORT neo_decoder {
public:
    static constexpr std::string_view name = "neo";
    static constexpr std::string_view extensions[] = {".neo"};

    /**
     * Check if data appears to be a NEO file.
     * @param data Raw file data
     * @return true if the signature matches NEO format
     */
    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;

    /**
     * Decode NEO image data to a surface.
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
// DEGAS Decoder (PI1/PI2/PI3 uncompressed, PC1/PC2/PC3 compressed)
// ============================================================================

class ONYX_IMAGE_EXPORT degas_decoder {
public:
    static constexpr std::string_view name = "degas";
    static constexpr std::string_view extensions[] = {
        ".pi1", ".pi2", ".pi3",  // Uncompressed
        ".pc1", ".pc2", ".pc3"   // Compressed
    };

    /**
     * Check if data appears to be a DEGAS file.
     * @param data Raw file data
     * @return true if the signature matches DEGAS format
     */
    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;

    /**
     * Decode DEGAS image data to a surface.
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
// Doodle Decoder (.DOO)
// ============================================================================

class ONYX_IMAGE_EXPORT doodle_decoder {
public:
    static constexpr std::string_view name = "doodle";
    static constexpr std::string_view extensions[] = {".doo"};

    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;
    [[nodiscard]] static decode_result decode(std::span<const std::uint8_t> data,
                                               surface& surf,
                                               const decode_options& options = {});
};

// ============================================================================
// Crack Art Decoder (.CA1/.CA2/.CA3)
// ============================================================================

class ONYX_IMAGE_EXPORT crack_art_decoder {
public:
    static constexpr std::string_view name = "crack_art";
    static constexpr std::string_view extensions[] = {".ca1", ".ca2", ".ca3"};

    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;
    [[nodiscard]] static decode_result decode(std::span<const std::uint8_t> data,
                                               surface& surf,
                                               const decode_options& options = {});
};

// ============================================================================
// Tiny Stuff Decoder (.TN1/.TN2/.TN3)
// ============================================================================

class ONYX_IMAGE_EXPORT tiny_stuff_decoder {
public:
    static constexpr std::string_view name = "tiny_stuff";
    static constexpr std::string_view extensions[] = {".tn1", ".tn2", ".tn3"};

    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;
    [[nodiscard]] static decode_result decode(std::span<const std::uint8_t> data,
                                               surface& surf,
                                               const decode_options& options = {});
};

// ============================================================================
// Spectrum 512 Decoder (.SPU/.SPC)
// ============================================================================

class ONYX_IMAGE_EXPORT spectrum512_decoder {
public:
    static constexpr std::string_view name = "spectrum512";
    static constexpr std::string_view extensions[] = {".spu", ".spc"};

    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;
    [[nodiscard]] static decode_result decode(std::span<const std::uint8_t> data,
                                               surface& surf,
                                               const decode_options& options = {});
};

// ============================================================================
// Photochrome Decoder (.PCS)
// ============================================================================

class ONYX_IMAGE_EXPORT photochrome_decoder {
public:
    static constexpr std::string_view name = "photochrome";
    static constexpr std::string_view extensions[] = {".pcs"};

    [[nodiscard]] static bool sniff(std::span<const std::uint8_t> data) noexcept;
    [[nodiscard]] static decode_result decode(std::span<const std::uint8_t> data,
                                               surface& surf,
                                               const decode_options& options = {});
};

} // namespace onyx_image

#endif // ONYX_IMAGE_CODECS_ATARIST_HPP_
