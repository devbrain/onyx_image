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

void test_interpaint_decode_md5(
    const char* filename,
    const char* expected_md5)
{
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "interpaint" / filename;

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
// InterPaint Decoder Tests
// ============================================================================

TEST_CASE("InterPaint decoder: sniff") {
    SUBCASE("Valid IPH file (9002 bytes)") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "interpaint" / "abydos.iph";
        auto data = read_file(path);
        REQUIRE(!data.empty());
        CHECK(onyx_image::interpaint_decoder::sniff(data));
    }

    SUBCASE("Valid IPT file (10003 bytes)") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "interpaint" / "abydos.ipt";
        auto data = read_file(path);
        REQUIRE(!data.empty());
        CHECK(onyx_image::interpaint_decoder::sniff(data));
    }

    SUBCASE("Invalid - too short") {
        std::vector<std::uint8_t> data = {0x00, 0x5c, 0x00, 0x00};
        CHECK_FALSE(onyx_image::interpaint_decoder::sniff(data));
    }

    SUBCASE("Invalid - wrong size") {
        std::vector<std::uint8_t> data(5000, 0);
        CHECK_FALSE(onyx_image::interpaint_decoder::sniff(data));
    }

    SUBCASE("Not confused with PNG") {
        std::vector<std::uint8_t> data = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        CHECK_FALSE(onyx_image::interpaint_decoder::sniff(data));
    }

    SUBCASE("Not confused with BMP") {
        std::vector<std::uint8_t> data = {'B', 'M', 0x00, 0x00, 0x00, 0x00};
        CHECK_FALSE(onyx_image::interpaint_decoder::sniff(data));
    }
}

TEST_CASE("InterPaint decoder: IPH hires files") {
    SUBCASE("abydos.iph") {
        test_interpaint_decode_md5("abydos.iph", "2f4c9de35a68c3f9c832ec44aa568185");
    }

    SUBCASE("INTERPHIRES.IPH") {
        test_interpaint_decode_md5("INTERPHIRES.IPH", "3d974017c3c5830771f8bbaae9e686b8");
    }

    SUBCASE("MEN_OR_MAN_.IPH") {
        test_interpaint_decode_md5("MEN_OR_MAN_.IPH", "6fba14b0250ae50f78992eaafbf2f7f1");
    }
}

TEST_CASE("InterPaint decoder: IPT multicolor files") {
    SUBCASE("abydos.ipt") {
        test_interpaint_decode_md5("abydos.ipt", "27441fa1005e0dbd8a6a33302424d02a");
    }

    SUBCASE("Samar.ipt") {
        test_interpaint_decode_md5("Samar.ipt", "d67bef4f13ba9030ee59379e96ca6273");
    }
}

TEST_CASE("InterPaint decoder: dimensions and format") {
    SUBCASE("IPH hires format") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "interpaint" / "abydos.iph";
        auto data = read_file(path);
        REQUIRE(!data.empty());

        onyx_image::memory_surface surface;
        auto result = onyx_image::interpaint_decoder::decode(data, surface);

        REQUIRE(result.ok);

        // InterPaint hires is 320x200
        CHECK(surface.width() == 320);
        CHECK(surface.height() == 200);
        CHECK(surface.format() == onyx_image::pixel_format::rgb888);

        // Check pixel data size: 320 * 200 * 3 bytes (RGB)
        CHECK(surface.pixels().size() == 320 * 200 * 3);
    }

    SUBCASE("IPT multicolor format") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "interpaint" / "abydos.ipt";
        auto data = read_file(path);
        REQUIRE(!data.empty());

        onyx_image::memory_surface surface;
        auto result = onyx_image::interpaint_decoder::decode(data, surface);

        REQUIRE(result.ok);

        // InterPaint multicolor is 320x200 (displayed with 2:1 aspect)
        CHECK(surface.width() == 320);
        CHECK(surface.height() == 200);
        CHECK(surface.format() == onyx_image::pixel_format::rgb888);

        // Check pixel data size: 320 * 200 * 3 bytes (RGB)
        CHECK(surface.pixels().size() == 320 * 200 * 3);
    }
}

TEST_CASE("InterPaint decoder: error handling") {
    SUBCASE("Empty data") {
        std::vector<std::uint8_t> data;
        onyx_image::memory_surface surface;
        auto result = onyx_image::interpaint_decoder::decode(data, surface);
        CHECK_FALSE(result.ok);
    }

    SUBCASE("Truncated data") {
        std::vector<std::uint8_t> data(100, 0);
        onyx_image::memory_surface surface;
        auto result = onyx_image::interpaint_decoder::decode(data, surface);
        CHECK_FALSE(result.ok);
    }

    SUBCASE("Dimension limits exceeded") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "interpaint" / "abydos.iph";
        auto data = read_file(path);
        REQUIRE(!data.empty());

        onyx_image::memory_surface surface;
        onyx_image::decode_options opts;
        opts.max_width = 100;  // InterPaint is 320 wide, so this should fail
        opts.max_height = 100;

        auto result = onyx_image::interpaint_decoder::decode(data, surface, opts);
        CHECK_FALSE(result.ok);
        CHECK(result.error == onyx_image::decode_error::dimensions_exceeded);
    }
}
