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

void test_pcx_decode_md5(
    const char* filename,
    const char* expected_md5,
    const char* format_name)
{
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "pcx" / filename;

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

TEST_CASE("PCX decoder: MD5 verification") {
    SUBCASE("Monochrome (1bpp, 1 plane)") {
        test_pcx_decode_md5("CGA_BW.PCX", "a2aed59abe356435c7ea3ed7c083e6ee", "Monochrome");
    }

    SUBCASE("CGA 4-color packed (2bpp, 1 plane)") {
        test_pcx_decode_md5("CGA_TST1.PCX", "20aacdfe3960e085aeea49e12a63d522", "CGA 4-color packed");
    }

    SUBCASE("CGA 4-color planar (1bpp, 2 planes)") {
        test_pcx_decode_md5("lena7.pcx", "9f4e84ed8100c92eb026da94226fa805", "CGA 4-color planar");
    }

    SUBCASE("EGA 8-color (1bpp, 3 planes)") {
        test_pcx_decode_md5("lena6.pcx", "7dca5f60e662946d7c7892ef0db95226", "EGA 8-color");
    }

    SUBCASE("EGA 16-color (1bpp, 4 planes)") {
        test_pcx_decode_md5("lena4.pcx", "b2ebd2077a67e52aea21e2a4a263b01c", "EGA 16-color");
    }

    SUBCASE("16-color packed (4bpp, 1 plane)") {
        test_pcx_decode_md5("lena10.pcx", "7dca5f60e662946d7c7892ef0db95226", "16-color packed");
    }

    SUBCASE("VGA 256-color (8bpp, 1 plane)") {
        test_pcx_decode_md5("SW0024.PCX", "23d5ee3fb86398a4952239bc99e6d3aa", "VGA 256-color");
    }

    SUBCASE("RGB 24-bit (8bpp, 3 planes)") {
        test_pcx_decode_md5("lena.pcx", "de01b43e0efbc4280aaf44b70dfc3f0e", "RGB 24-bit");
    }
}
