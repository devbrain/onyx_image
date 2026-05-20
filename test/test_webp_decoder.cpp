#include <doctest/doctest.h>
#include <onyx_image/onyx_image.hpp>

#include <array>
#include <cstdint>

namespace {

constexpr std::array<std::uint8_t, 36> RED_2X2_WEBP = {
    0x52, 0x49, 0x46, 0x46, 0x1c, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50,
    0x56, 0x50, 0x38, 0x4c, 0x0f, 0x00, 0x00, 0x00, 0x2f, 0x01, 0x40, 0x00,
    0x00, 0x07, 0x10, 0xf5, 0x8f, 0xfe, 0x07, 0x22, 0xa2, 0xff, 0x01, 0x00,
};

} // namespace

TEST_CASE("WebP decoder: sniff") {
    SUBCASE("Valid WebP signature") {
        CHECK(onyx_image::webp_decoder::sniff(RED_2X2_WEBP));
    }

    SUBCASE("Invalid signature") {
        constexpr std::array<std::uint8_t, 12> data = {
            'R', 'I', 'F', 'F', 0x00, 0x00, 0x00, 0x00, 'W', 'A', 'V', 'E',
        };
        CHECK_FALSE(onyx_image::webp_decoder::sniff(data));
    }

    SUBCASE("Too short") {
        constexpr std::array<std::uint8_t, 4> data = {'R', 'I', 'F', 'F'};
        CHECK_FALSE(onyx_image::webp_decoder::sniff(data));
    }
}

TEST_CASE("WebP decoder: decode lossless RGBA") {
    onyx_image::memory_surface surface;
    auto result = onyx_image::decode(RED_2X2_WEBP, surface);

    REQUIRE(result.ok);
    CHECK(surface.width() == 2);
    CHECK(surface.height() == 2);
    CHECK(surface.format() == onyx_image::pixel_format::rgba8888);

    const auto pixels = surface.pixels();
    REQUIRE(pixels.size() == 16);
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(pixels[i * 4 + 0] == 254);
        CHECK(pixels[i * 4 + 1] == 0);
        CHECK(pixels[i * 4 + 2] == 0);
        CHECK(pixels[i * 4 + 3] == 255);
    }
}

TEST_CASE("WebP decoder: dimension limits") {
    onyx_image::decode_options options;
    options.max_width = 1;
    options.max_height = 2;

    onyx_image::memory_surface surface;
    auto result = onyx_image::webp_decoder::decode(RED_2X2_WEBP, surface, options);

    CHECK_FALSE(result.ok);
    CHECK(result.error == onyx_image::decode_error::dimensions_exceeded);
}
