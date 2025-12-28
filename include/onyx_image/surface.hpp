#ifndef ONYX_IMAGE_SURFACE_HPP_
#define ONYX_IMAGE_SURFACE_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace onyx_image {

// ============================================================================
// Surface Interface
// ============================================================================

/**
 * Abstract base class for image surfaces.
 * Decoders write pixels to surfaces, allowing framework-agnostic decoding.
 *
 * Implement this interface to integrate with your rendering framework
 * (e.g., SDL_Surface, SDL_Texture, OpenGL texture, etc.)
 */
class ONYX_IMAGE_EXPORT surface {
public:
    virtual ~surface() = default;

    /**
     * Set the surface dimensions and pixel format.
     * Called before any pixel writes.
     * @param width Image width in pixels
     * @param height Image height in pixels
     * @param format Pixel format
     * @return true if allocation succeeded
     */
    virtual bool set_size(int width, int height, pixel_format format) = 0;

    /**
     * Write a horizontal run of pixel data.
     *
     * NOTE: The x parameter is a BYTE OFFSET within the row, not a pixel coordinate.
     * For RGB formats, use x = pixel_x * 3; for RGBA, use x = pixel_x * 4.
     *
     * @param x Starting byte offset within the row (NOT pixel coordinate)
     * @param y Y coordinate (row number)
     * @param count Number of bytes to write
     * @param pixels Pointer to pixel data
     */
    virtual void write_pixels(int x, int y, int count, const std::uint8_t* pixels) = 0;

    /**
     * Write a single pixel (for indexed8 format).
     * @param x X coordinate
     * @param y Y coordinate
     * @param pixel Pixel value (palette index for indexed8)
     */
    virtual void write_pixel(int x, int y, std::uint8_t pixel) = 0;

    /**
     * Set the palette size (for indexed formats).
     * @param count Number of palette entries (max 256)
     */
    virtual void set_palette_size(int count) { (void)count; }

    /**
     * Write palette entries.
     * @param start Starting palette index
     * @param colors RGB triplets (3 bytes per color)
     */
    virtual void write_palette(int start, std::span<const std::uint8_t> colors) {
        (void)start;
        (void)colors;
    }

    /**
     * Set a subrect for multi-image containers.
     * @param index Subrect index
     * @param sr Subrect metadata
     */
    virtual void set_subrect(int index, const subrect& sr) {
        (void)index;
        (void)sr;
    }
};

// ============================================================================
// Memory Surface (default implementation)
// ============================================================================

/**
 * Simple in-memory surface implementation.
 * Stores pixels in a contiguous buffer with optional palette.
 */
class ONYX_IMAGE_EXPORT memory_surface : public surface {
public:
    memory_surface() = default;
    ~memory_surface() override = default;

    memory_surface(const memory_surface&) = delete;
    memory_surface& operator=(const memory_surface&) = delete;
    memory_surface(memory_surface&&) noexcept = default;
    memory_surface& operator=(memory_surface&&) noexcept = default;

    // Surface interface
    bool set_size(int width, int height, pixel_format format) override;
    void write_pixels(int x, int y, int count, const std::uint8_t* pixels) override;
    void write_pixel(int x, int y, std::uint8_t pixel) override;
    void set_palette_size(int count) override;
    void write_palette(int start, std::span<const std::uint8_t> colors) override;
    void set_subrect(int index, const subrect& sr) override;

    // Accessors (read-only)
    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] pixel_format format() const noexcept { return format_; }
    [[nodiscard]] std::span<const std::uint8_t> pixels() const noexcept { return pixels_; }
    [[nodiscard]] std::span<const std::uint8_t> palette() const noexcept { return palette_; }
    [[nodiscard]] const std::vector<subrect>& subrects() const noexcept { return subrects_; }
    [[nodiscard]] std::size_t pitch() const noexcept { return pitch_; }

    // Mutable accessors (for post-decode manipulation)
    [[nodiscard]] std::span<std::uint8_t> mutable_pixels() noexcept { return pixels_; }
    [[nodiscard]] std::span<std::uint8_t> mutable_palette() noexcept { return palette_; }

private:
    std::vector<std::uint8_t> pixels_;
    std::vector<std::uint8_t> palette_;  // RGB triplets
    std::vector<subrect> subrects_;
    int width_ = 0;
    int height_ = 0;
    std::size_t pitch_ = 0;
    pixel_format format_ = pixel_format::rgba8888;
};

} // namespace onyx_image

#endif // ONYX_IMAGE_SURFACE_HPP_
