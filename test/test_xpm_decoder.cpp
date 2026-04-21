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

    const int width = surf.width();
    const int height = surf.height();
    const auto format = static_cast<int>(surf.format());
    MD5_Update(&ctx, &width, sizeof(width));
    MD5_Update(&ctx, &height, sizeof(height));
    MD5_Update(&ctx, &format, sizeof(format));

    const auto pixels = surf.pixels();
    MD5_Update(&ctx, pixels.data(), pixels.size());

    if (surf.format() == onyx_image::pixel_format::indexed8) {
        const auto palette = surf.palette();
        MD5_Update(&ctx, palette.data(), palette.size());
    }

    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_Final(digest, &ctx);

    return md5_to_string(digest);
}

void test_xpm_decode_md5(const char* filename, const char* expected_md5, const char* label) {
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / filename;

    INFO("Testing: ", filename, " (", label, ")");
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

TEST_CASE("XPM decoder: sniff") {
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "xpm" / "teapot.xpm";
    auto data = read_file(path);
    REQUIRE(!data.empty());
    CHECK(onyx_image::xpm_decoder::sniff(data));

    std::vector<std::uint8_t> invalid = {'X', 'P', 'M', '\n'};
    CHECK_FALSE(onyx_image::xpm_decoder::sniff(invalid));
}

TEST_CASE("XPM decoder: MD5 verification") {
    test_xpm_decode_md5("xpm/teapot.xpm", "8bf4ce95b071271c2084baf481e9eff5", "Teapot XPM");
}
