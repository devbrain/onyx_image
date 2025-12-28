#include <doctest/doctest.h>
#include <onyx_image/onyx_image.hpp>

#include "helpers/md5.h"

#include <cstdio>
#include <cstring>
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

    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_Final(digest, &ctx);

    return md5_to_string(digest);
}

void test_koala_decode_md5(
    const char* filename,
    const char* expected_md5)
{
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "koala" / filename;

    INFO("Testing: ", filename);
    REQUIRE(std::filesystem::exists(path));

    auto data = read_file(path);
    REQUIRE(!data.empty());

    onyx_image::memory_surface surface;
    auto result = onyx_image::decode(data, surface);

    REQUIRE(result.ok);
    CHECK(surface.width() == 320);
    CHECK(surface.height() == 200);
    CHECK(surface.format() == onyx_image::pixel_format::rgb888);

    std::string actual_md5 = compute_surface_md5(surface);
    CHECK(actual_md5 == expected_md5);
}

} // namespace

// ============================================================================
// Koala Decoder Tests
// ============================================================================

TEST_CASE("Koala decoder: sniff") {
    SUBCASE("Valid uncompressed Koala (10003 bytes)") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "koala" / "abydos.koa";
        auto data = read_file(path);
        REQUIRE(!data.empty());
        CHECK(onyx_image::koala_decoder::sniff(data));
    }

    SUBCASE("Valid GG compressed Koala") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "koala" / "abydos.gg";
        auto data = read_file(path);
        REQUIRE(!data.empty());
        CHECK(onyx_image::koala_decoder::sniff(data));
    }

    SUBCASE("Invalid - too short") {
        std::vector<std::uint8_t> data = {0x00, 0x60, 0x00, 0x00};
        CHECK_FALSE(onyx_image::koala_decoder::sniff(data));
    }

    SUBCASE("Invalid - wrong size") {
        std::vector<std::uint8_t> data(5000, 0);
        data[0] = 0x00;
        data[1] = 0x60;
        CHECK_FALSE(onyx_image::koala_decoder::sniff(data));
    }

    SUBCASE("Not confused with PNG") {
        std::vector<std::uint8_t> data = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        CHECK_FALSE(onyx_image::koala_decoder::sniff(data));
    }

    SUBCASE("Not confused with BMP") {
        std::vector<std::uint8_t> data = {'B', 'M', 0x00, 0x00, 0x00, 0x00};
        CHECK_FALSE(onyx_image::koala_decoder::sniff(data));
    }
}

TEST_CASE("Koala decoder: uncompressed KOA files") {
    SUBCASE("abydos.koa") {
        test_koala_decode_md5("abydos.koa", "27441fa1005e0dbd8a6a33302424d02a");
    }

    SUBCASE("NINJA3.KOA") {
        test_koala_decode_md5("NINJA3.KOA", "1a77c14098d94e37cf783357a75e9213");
    }

    SUBCASE("GPANTHE.gig") {
        test_koala_decode_md5("GPANTHE.gig", "2cef507fe6322f0a243fb27be4dfa30c");
    }
}

TEST_CASE("Koala decoder: GG compressed files") {
    SUBCASE("abydos.gg") {
        // Should produce identical output to abydos.koa
        test_koala_decode_md5("abydos.gg", "27441fa1005e0dbd8a6a33302424d02a");
    }

    SUBCASE("ggbikini") {
        test_koala_decode_md5("ggbikini", "342147b0ac0a817ea131c1703965b9d9");
    }

    SUBCASE("ggblonde") {
        test_koala_decode_md5("ggblonde", "7effbd04cfb23112d392b2787a914b0d");
    }

    SUBCASE("GGFAT.GG") {
        test_koala_decode_md5("GGFAT.GG", "cebda54e70c2fa2a56249d02c278d5c5");
    }

    SUBCASE("GGLUMLITE.GG") {
        test_koala_decode_md5("GGLUMLITE.GG", "2b8d1ef7952e8afe55706a0d672d2a48");
    }

    SUBCASE("ggspazoz") {
        test_koala_decode_md5("ggspazoz", "1db387f912f71222acbe393805a8aa42");
    }
}

TEST_CASE("Koala decoder: .koala extension files") {
    SUBCASE("paralax.koala") {
        test_koala_decode_md5("paralax.koala", "567f1ea0c3b36c0268a5b99f871b29fd");
    }

    SUBCASE("parallax.koala") {
        test_koala_decode_md5("parallax.koala", "dad3e6cd545b63516132165c80e9ccc0");
    }
}

TEST_CASE("Koala decoder: GG and KOA produce identical output") {
    // abydos exists in both compressed and uncompressed form
    const std::filesystem::path koa_path = std::filesystem::path(TEST_DATA_DIR) / "koala" / "abydos.koa";
    const std::filesystem::path gg_path = std::filesystem::path(TEST_DATA_DIR) / "koala" / "abydos.gg";

    auto koa_data = read_file(koa_path);
    auto gg_data = read_file(gg_path);
    REQUIRE(!koa_data.empty());
    REQUIRE(!gg_data.empty());

    onyx_image::memory_surface koa_surface;
    onyx_image::memory_surface gg_surface;

    auto koa_result = onyx_image::decode(koa_data, koa_surface);
    auto gg_result = onyx_image::decode(gg_data, gg_surface);

    REQUIRE(koa_result.ok);
    REQUIRE(gg_result.ok);

    // Same dimensions
    CHECK(koa_surface.width() == gg_surface.width());
    CHECK(koa_surface.height() == gg_surface.height());
    CHECK(koa_surface.format() == gg_surface.format());

    // Same pixel data
    auto koa_pixels = koa_surface.pixels();
    auto gg_pixels = gg_surface.pixels();
    CHECK(koa_pixels.size() == gg_pixels.size());
    CHECK(std::memcmp(koa_pixels.data(), gg_pixels.data(), koa_pixels.size()) == 0);
}
