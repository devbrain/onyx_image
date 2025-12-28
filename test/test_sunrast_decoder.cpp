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

void test_sunrast_decode_md5(
    const char* filename,
    const char* expected_md5,
    const char* format_name)
{
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "sunrast" / filename;

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

TEST_CASE("Sun Raster decoder: sniff") {
    SUBCASE("Valid Sun Raster signature") {
        std::vector<std::uint8_t> data = {0x59, 0xa6, 0x6a, 0x95};
        CHECK(onyx_image::sunrast_decoder::sniff(data));
    }

    SUBCASE("Invalid signature") {
        std::vector<std::uint8_t> data = {0x89, 'P', 'N', 'G'};
        CHECK_FALSE(onyx_image::sunrast_decoder::sniff(data));
    }

    SUBCASE("Too short") {
        std::vector<std::uint8_t> data = {0x59, 0xa6};
        CHECK_FALSE(onyx_image::sunrast_decoder::sniff(data));
    }
}

TEST_CASE("Sun Raster decoder: MD5 verification") {
    SUBCASE("1-bit raw (lena-1bit-raw.sun)") {
        test_sunrast_decode_md5("lena-1bit-raw.sun", "5916d7d48cdc1b4570fd82f3bb916cc3", "1-bit raw");
    }

    SUBCASE("1-bit RLE (lena-1bit-rle.sun)") {
        test_sunrast_decode_md5("lena-1bit-rle.sun", "5916d7d48cdc1b4570fd82f3bb916cc3", "1-bit RLE");
    }

    SUBCASE("8-bit raw (lena-8bit-raw.sun)") {
        test_sunrast_decode_md5("lena-8bit-raw.sun", "ddb0296d49763d0e35c66c601f0a5cf6", "8-bit raw");
    }

    SUBCASE("8-bit RLE (lena-8bit-rle.sun)") {
        test_sunrast_decode_md5("lena-8bit-rle.sun", "ddb0296d49763d0e35c66c601f0a5cf6", "8-bit RLE");
    }

    SUBCASE("24-bit raw (lena-24bit-raw.sun)") {
        test_sunrast_decode_md5("lena-24bit-raw.sun", "267a484483e279458e95b972c6c27cd3", "24-bit raw");
    }

    SUBCASE("24-bit RLE (lena-24bit-rle.sun)") {
        test_sunrast_decode_md5("lena-24bit-rle.sun", "267a484483e279458e95b972c6c27cd3", "24-bit RLE");
    }

    SUBCASE("4-bit indexed (4bpp.ras)") {
        test_sunrast_decode_md5("4bpp.ras", "2de0de85e581628c1aabf1d9f568a0d2", "4-bit indexed");
    }

    SUBCASE("32-bit (32bpp.ras)") {
        test_sunrast_decode_md5("32bpp.ras", "c69dbe173cabb2aa858aaa8aa83451a7", "32-bit");
    }
}
