#include <onyx_image/surface.hpp>

#include <algorithm>
#include <cstring>
#include <limits>

namespace onyx_image {

bool memory_surface::set_size(int width, int height, pixel_format format) {
    if (width <= 0 || height <= 0) {
        return false;
    }

    const std::size_t w = static_cast<std::size_t>(width);
    const std::size_t h = static_cast<std::size_t>(height);
    const std::size_t bpp = bytes_per_pixel(format);

    // Check for overflow in pitch calculation (width * bpp)
    if (w > std::numeric_limits<std::size_t>::max() / bpp) {
        return false;
    }
    const std::size_t pitch = w * bpp;

    // Check for overflow in total size calculation (pitch * height)
    if (pitch > std::numeric_limits<std::size_t>::max() / h) {
        return false;
    }
    const std::size_t total_size = pitch * h;

    // Additional sanity check - limit to reasonable maximum (1GB)
    constexpr std::size_t MAX_BUFFER_SIZE = 1024ULL * 1024ULL * 1024ULL;
    if (total_size > MAX_BUFFER_SIZE) {
        return false;
    }

    width_ = width;
    height_ = height;
    format_ = format;
    pitch_ = pitch;

    try {
        pixels_.resize(total_size);
        std::fill(pixels_.begin(), pixels_.end(), 0);
    } catch (const std::bad_alloc&) {
        return false;
    }

    palette_.clear();
    subrects_.clear();

    return true;
}

void memory_surface::write_pixels(int x, int y, int count, const std::uint8_t* pixels) {
    if (y < 0 || y >= height_ || x < 0 || count <= 0 || !pixels) {
        return;
    }

    const std::size_t x_offset = static_cast<std::size_t>(x);

    // Guard against x >= pitch_ to prevent underflow in max_bytes calculation
    if (x_offset >= pitch_) {
        return;
    }

    const std::size_t offset = static_cast<std::size_t>(y) * pitch_ + x_offset;
    const std::size_t max_bytes = pitch_ - x_offset;
    const std::size_t bytes_to_copy = std::min(static_cast<std::size_t>(count), max_bytes);

    // Final bounds check before memcpy
    if (offset + bytes_to_copy <= pixels_.size()) {
        std::memcpy(pixels_.data() + offset, pixels, bytes_to_copy);
    }
}

void memory_surface::write_pixel(int x, int y, std::uint8_t pixel) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) {
        return;
    }

    if (format_ != pixel_format::indexed8) {
        return;  // write_pixel only works for indexed8
    }

    const std::size_t offset = static_cast<std::size_t>(y) * pitch_ + static_cast<std::size_t>(x);
    if (offset < pixels_.size()) {
        pixels_[offset] = pixel;
    }
}

void memory_surface::set_palette_size(int count) {
    if (count <= 0 || count > 256) {
        return;
    }
    palette_.resize(static_cast<std::size_t>(count) * 3);
    std::fill(palette_.begin(), palette_.end(), 0);
}

void memory_surface::write_palette(int start, std::span<const std::uint8_t> colors) {
    if (start < 0 || colors.empty()) {
        return;
    }

    const std::size_t start_offset = static_cast<std::size_t>(start) * 3;
    if (start_offset >= palette_.size()) {
        return;
    }

    const std::size_t bytes_to_copy = std::min(colors.size(), palette_.size() - start_offset);
    std::memcpy(palette_.data() + start_offset, colors.data(), bytes_to_copy);
}

void memory_surface::set_subrect(int index, const subrect& sr) {
    if (index < 0) {
        return;
    }

    if (static_cast<std::size_t>(index) >= subrects_.size()) {
        subrects_.resize(static_cast<std::size_t>(index) + 1);
    }
    subrects_[static_cast<std::size_t>(index)] = sr;
}

} // namespace onyx_image
