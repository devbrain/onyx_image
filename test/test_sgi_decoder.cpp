#include <doctest/doctest.h>
#include <onyx_image/onyx_image.hpp>

#include "helpers/md5.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }

    const auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), size);

    return data;
}

std::string md5_to_string(const unsigned char* digest) {
    std::string result;
    result.reserve(32);
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        char buf[3];
        std::snprintf(buf, sizeof(buf), "%02x", digest[i]);
        result += buf;
    }
    return result;
}

std::string compute_surface_md5(const onyx_image::memory_surface& surf) {
    MD5_CTX ctx;
    MD5_Init(&ctx);

    // Hash dimensions and format
    const int width = surf.width();
    const int height = surf.height();
    const auto format = static_cast<int>(surf.format());
    MD5_Update(&ctx, &width, sizeof(width));
    MD5_Update(&ctx, &height, sizeof(height));
    MD5_Update(&ctx, &format, sizeof(format));

    // Hash pixel data
    const auto pixels = surf.pixels();
    MD5_Update(&ctx, pixels.data(), pixels.size());

    // Hash palette if indexed
    if (surf.format() == onyx_image::pixel_format::indexed8) {
        const auto palette = surf.palette();
        MD5_Update(&ctx, palette.data(), palette.size());
    }

    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_Final(digest, &ctx);

    return md5_to_string(digest);
}

void test_sgi_decode_md5(
    const char* filename,
    const char* expected_md5,
    const char* format_name)
{
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / filename;

    INFO("Testing: ", filename, " (", format_name, ")");
    REQUIRE(std::filesystem::exists(path));

    auto data = read_file(path);
    REQUIRE(!data.empty());

    onyx_image::memory_surface surface;
    auto result = onyx_image::decode(data, surface);

    REQUIRE(result.ok);
    REQUIRE(surface.width() > 0);
    REQUIRE(surface.height() > 0);

    std::string actual_md5 = compute_surface_md5(surface);
    CHECK(actual_md5 == expected_md5);
}

} // namespace

TEST_CASE("SGI decoder: sniff") {
    SUBCASE("Valid SGI signature") {
        std::vector<std::uint8_t> data = {0x01, 0xDA};
        CHECK(onyx_image::sgi_decoder::sniff(data));
    }

    SUBCASE("Invalid signature") {
        std::vector<std::uint8_t> data = {0x89, 'P', 'N', 'G'};
        CHECK_FALSE(onyx_image::sgi_decoder::sniff(data));
    }

    SUBCASE("Too short") {
        std::vector<std::uint8_t> data = {0x01};
        CHECK_FALSE(onyx_image::sgi_decoder::sniff(data));
    }
}

TEST_CASE("SGI decoder: MD5 verification") {
    SUBCASE("RGB 24-bit uncompressed") {
        test_sgi_decode_md5("sgi/rgb24.sgi", "3dc1ecc04b28fd2f2be448ceaaca5a74", "RGB 24-bit uncompressed");
    }

    SUBCASE("RGB 24-bit RLE") {
        test_sgi_decode_md5("sgi/rgb24rle.sgi", "3dc1ecc04b28fd2f2be448ceaaca5a74", "RGB 24-bit RLE");
    }

    SUBCASE("RGBA 32-bit RLE") {
        test_sgi_decode_md5("sgi/rgb24alpharle.sgi", "e236bd957a925fbac6bba117eafa0312", "RGBA 32-bit RLE");
    }

    SUBCASE("Grayscale 8-bit uncompressed") {
        test_sgi_decode_md5("sgi/rgb8.sgi", "2a36fd472caf578e1251dc6b3749002a", "Grayscale 8-bit uncompressed");
    }

    SUBCASE("Grayscale 8-bit RLE") {
        test_sgi_decode_md5("sgi/rgb8rle.sgi", "2a36fd472caf578e1251dc6b3749002a", "Grayscale 8-bit RLE");
    }

    SUBCASE("Grayscale with alpha") {
        test_sgi_decode_md5("sgi/rgb8a.sgi", "2d0a1058e1c16c65c11dee23e285153b", "Grayscale with alpha");
    }

    SUBCASE("Grayscale RLE variant") {
        test_sgi_decode_md5("sgi/sgb8rle.sgi", "b0c432f83035765e0ad8b9da84c2b104", "Grayscale RLE variant");
    }
}
