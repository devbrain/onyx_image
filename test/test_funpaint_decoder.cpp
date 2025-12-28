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

void test_funpaint_decode_md5(
    const char* filename,
    const char* expected_md5)
{
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "funpaint" / filename;

    INFO("Testing: ", filename);
    REQUIRE(std::filesystem::exists(path));

    auto data = read_file(path);
    REQUIRE(!data.empty());

    onyx_image::memory_surface surface;
    auto result = onyx_image::decode(data, surface);

    REQUIRE(result.ok);
    CHECK(surface.width() == 296);
    CHECK(surface.height() == 200);
    CHECK(surface.format() == onyx_image::pixel_format::rgb888);

    std::string actual_md5 = compute_surface_md5(surface);
    CHECK(actual_md5 == expected_md5);
}

} // namespace

// ============================================================================
// FunPaint Decoder Tests
// ============================================================================

TEST_CASE("FunPaint decoder: sniff") {
    SUBCASE("Valid FunPaint file (uncompressed)") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "funpaint" / "Valsary.fun";
        auto data = read_file(path);
        REQUIRE(!data.empty());
        CHECK(onyx_image::funpaint_decoder::sniff(data));
    }

    SUBCASE("Valid FunPaint file (compressed)") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "funpaint" / "KATER.fp2";
        auto data = read_file(path);
        REQUIRE(!data.empty());
        CHECK(onyx_image::funpaint_decoder::sniff(data));
    }

    SUBCASE("Invalid - too short") {
        std::vector<std::uint8_t> data = {0x00, 0x3f, 'F', 'U', 'N'};
        CHECK_FALSE(onyx_image::funpaint_decoder::sniff(data));
    }

    SUBCASE("Invalid - wrong signature") {
        std::vector<std::uint8_t> data(100, 0);
        data[0] = 0xf0;
        data[1] = 0x3f;
        std::memcpy(data.data() + 2, "NOTFUNPAINT!!!", 14);
        CHECK_FALSE(onyx_image::funpaint_decoder::sniff(data));
    }

    SUBCASE("Not confused with PNG") {
        std::vector<std::uint8_t> data = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        CHECK_FALSE(onyx_image::funpaint_decoder::sniff(data));
    }

    SUBCASE("Not confused with BMP") {
        std::vector<std::uint8_t> data = {'B', 'M', 0x00, 0x00, 0x00, 0x00};
        CHECK_FALSE(onyx_image::funpaint_decoder::sniff(data));
    }
}

TEST_CASE("FunPaint decoder: uncompressed files") {
    SUBCASE("Valsary.fun") {
        test_funpaint_decode_md5("Valsary.fun", "9cfa79f4e8f83d9dbbd8998c646adcf5");
    }

    SUBCASE("Viking.fun") {
        test_funpaint_decode_md5("Viking.fun", "29b4fb9db26455a9d4b626cb481d8833");
    }

    SUBCASE("Propaganda15.vic") {
        test_funpaint_decode_md5("Propaganda15.vic", "261091c37d568eb246b31fd29be2e6d0");
    }
}

TEST_CASE("FunPaint decoder: compressed files") {
    SUBCASE("KATER.fp2") {
        test_funpaint_decode_md5("KATER.fp2", "290aa94497f5f0cd216cbd92fb975ba5");
    }

    SUBCASE("a_dettke.fp") {
        test_funpaint_decode_md5("a_dettke.fp", "2fdb1c2747f676d802fc5a0ba02e22df");
    }

    SUBCASE("benz.fp") {
        test_funpaint_decode_md5("benz.fp", "cbd8ec3d2327d4cdeef764423fbe7fd2");
    }

    SUBCASE("ferrari365.fp") {
        test_funpaint_decode_md5("ferrari365.fp", "df4c84c86b6160002714e239563f31f2");
    }
}

TEST_CASE("FunPaint decoder: dimensions and format") {
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "funpaint" / "Valsary.fun";
    auto data = read_file(path);
    REQUIRE(!data.empty());

    onyx_image::memory_surface surface;
    auto result = onyx_image::funpaint_decoder::decode(data, surface);

    REQUIRE(result.ok);

    // FunPaint IFLI is 296x200 (FLI bug removes 24 pixels from left)
    CHECK(surface.width() == 296);
    CHECK(surface.height() == 200);
    CHECK(surface.format() == onyx_image::pixel_format::rgb888);

    // Check pixel data size: 296 * 200 * 3 bytes (RGB)
    CHECK(surface.pixels().size() == 296 * 200 * 3);
}

TEST_CASE("FunPaint decoder: error handling") {
    SUBCASE("Empty data") {
        std::vector<std::uint8_t> data;
        onyx_image::memory_surface surface;
        auto result = onyx_image::funpaint_decoder::decode(data, surface);
        CHECK_FALSE(result.ok);
    }

    SUBCASE("Truncated header") {
        std::vector<std::uint8_t> data(10, 0);
        onyx_image::memory_surface surface;
        auto result = onyx_image::funpaint_decoder::decode(data, surface);
        CHECK_FALSE(result.ok);
    }

    SUBCASE("Invalid signature") {
        std::vector<std::uint8_t> data(100, 0);
        data[0] = 0xf0;
        data[1] = 0x3f;
        std::memcpy(data.data() + 2, "NOTFUNPAINT!!!", 14);
        onyx_image::memory_surface surface;
        auto result = onyx_image::funpaint_decoder::decode(data, surface);
        CHECK_FALSE(result.ok);
    }

    SUBCASE("Dimension limits exceeded") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "funpaint" / "Valsary.fun";
        auto data = read_file(path);
        REQUIRE(!data.empty());

        onyx_image::memory_surface surface;
        onyx_image::decode_options opts;
        opts.max_width = 100;  // FunPaint is 296 wide, so this should fail
        opts.max_height = 100;

        auto result = onyx_image::funpaint_decoder::decode(data, surface, opts);
        CHECK_FALSE(result.ok);
        CHECK(result.error == onyx_image::decode_error::dimensions_exceeded);
    }
}
