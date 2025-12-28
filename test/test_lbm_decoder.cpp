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

void test_lbm_decode_md5(
    const char* filename,
    const char* expected_md5,
    const char* format_name)
{
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "lbm" / filename;

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

TEST_CASE("LBM decoder: sniff") {
    SUBCASE("Valid ILBM signature") {
        // "FORM" + size + "ILBM"
        std::vector<std::uint8_t> data = {
            'F', 'O', 'R', 'M',
            0x00, 0x00, 0x00, 0x10,
            'I', 'L', 'B', 'M'
        };
        CHECK(onyx_image::lbm_decoder::sniff(data));
    }

    SUBCASE("Valid PBM signature") {
        std::vector<std::uint8_t> data = {
            'F', 'O', 'R', 'M',
            0x00, 0x00, 0x00, 0x10,
            'P', 'B', 'M', ' '
        };
        CHECK(onyx_image::lbm_decoder::sniff(data));
    }

    SUBCASE("Invalid signature") {
        std::vector<std::uint8_t> data = {
            'R', 'I', 'F', 'F',
            0x00, 0x00, 0x00, 0x10,
            'W', 'A', 'V', 'E'
        };
        CHECK_FALSE(onyx_image::lbm_decoder::sniff(data));
    }

    SUBCASE("Too short") {
        std::vector<std::uint8_t> data = {'F', 'O', 'R', 'M'};
        CHECK_FALSE(onyx_image::lbm_decoder::sniff(data));
    }
}

TEST_CASE("LBM decoder: MD5 verification") {
    SUBCASE("ILBM 8-bit indexed (rgb8c.ilbm)") {
        test_lbm_decode_md5("rgb8c.ilbm", "04e7d2e7e7f1bb6f1fc58390c20417da", "ILBM 8-bit indexed");
    }

    SUBCASE("ILBM 5-plane 32 colors (rockdudes.ilbm)") {
        test_lbm_decode_md5("rockdudes.ilbm", "aa6586e8be7893440072cbf45b594c7c", "ILBM 5-plane");
    }

    SUBCASE("ILBM 4-plane 16 colors (enterprise.iff)") {
        test_lbm_decode_md5("enterprise.iff", "4d7b6fd406675fba2e03795107a7a543", "ILBM 4-plane");
    }

    SUBCASE("ILBM 24-bit truecolor (ref.iff)") {
        test_lbm_decode_md5("ref.iff", "d1b27e3e51854e29981c7d550e26b26b", "ILBM 24-bit");
    }

    SUBCASE("ILBM HAM6 mode (crater.ham)") {
        test_lbm_decode_md5("crater.ham", "6625cb3cac7cd35603f99b96bfd2c70a", "ILBM HAM6");
    }

    SUBCASE("PBM chunky 8-bit (stone_circle.lbm)") {
        test_lbm_decode_md5("stone_circle.lbm", "3cd085dc39dee19e81f3018f8e121c90", "PBM chunky");
    }

    SUBCASE("ILBM uncompressed (rt32.iff)") {
        test_lbm_decode_md5("rt32.iff", "38fad3937a2448b019b1452b7ec90433", "ILBM uncompressed");
    }
}
