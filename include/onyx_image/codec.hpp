#ifndef ONYX_IMAGE_CODEC_HPP_
#define ONYX_IMAGE_CODEC_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace onyx_image {

// ============================================================================
// Decoder Interface
// ============================================================================

/**
 * Abstract base class for image decoders.
 * Used by the codec registry for runtime polymorphism.
 */
class ONYX_IMAGE_EXPORT decoder {
public:
    virtual ~decoder() = default;

    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual std::span<const std::string_view> extensions() const noexcept = 0;
    [[nodiscard]] virtual bool sniff(std::span<const std::uint8_t> data) const noexcept = 0;
    [[nodiscard]] virtual decode_result decode(std::span<const std::uint8_t> data,
                                                surface& surf,
                                                const decode_options& options) const = 0;
};

// ============================================================================
// Codec Registry
// ============================================================================

/**
 * Registry for image decoders.
 * Built-in codecs are registered by default.
 * User code can add new codecs at runtime.
 */
class ONYX_IMAGE_EXPORT codec_registry {
public:
    /**
     * Get the global codec registry instance.
     */
    [[nodiscard]] static codec_registry& instance();

    /**
     * Register a decoder.
     * @param dec Unique pointer to decoder (ownership transferred)
     */
    void register_decoder(std::unique_ptr<decoder> dec);

    /**
     * Find decoder by sniffing data.
     * @param data Raw file data
     * @return Pointer to decoder if found, nullptr otherwise
     */
    [[nodiscard]] const decoder* find_decoder(std::span<const std::uint8_t> data) const;

    /**
     * Find decoder by name.
     * @param name Codec name (e.g., "pcx")
     * @return Pointer to decoder if found, nullptr otherwise
     */
    [[nodiscard]] const decoder* find_decoder(std::string_view name) const;

    /**
     * Get number of registered decoders.
     */
    [[nodiscard]] std::size_t decoder_count() const noexcept {
        return decoders_.size();
    }

    /**
     * Get decoder at index.
     * @param index Decoder index (0 to decoder_count()-1)
     * @return Pointer to decoder, or nullptr if index out of range
     */
    [[nodiscard]] const decoder* decoder_at(std::size_t index) const noexcept {
        return index < decoders_.size() ? decoders_[index].get() : nullptr;
    }

private:
    codec_registry();
    ~codec_registry();

    codec_registry(const codec_registry&) = delete;
    codec_registry& operator=(const codec_registry&) = delete;

    void register_builtin_codecs();

    std::vector<std::unique_ptr<decoder>> decoders_;
};

// ============================================================================
// Convenience Decode Functions
// ============================================================================

/**
 * Decode image data to a surface (auto-detect format).
 * @param data Raw file data
 * @param surf Destination surface
 * @param options Decode options
 * @return Decode result
 */
[[nodiscard]] ONYX_IMAGE_EXPORT decode_result decode(std::span<const std::uint8_t> data,
                                                      surface& surf,
                                                      const decode_options& options = {});

/**
 * Decode image data to a surface (explicit codec).
 * @param data Raw file data
 * @param surf Destination surface
 * @param codec_name Name of codec to use
 * @param options Decode options
 * @return Decode result
 */
[[nodiscard]] ONYX_IMAGE_EXPORT decode_result decode(std::span<const std::uint8_t> data,
                                                      surface& surf,
                                                      std::string_view codec_name,
                                                      const decode_options& options = {});

} // namespace onyx_image

#endif // ONYX_IMAGE_CODEC_HPP_
