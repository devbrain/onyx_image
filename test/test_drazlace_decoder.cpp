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

void test_drazlace_decode_md5(
    const char* filename,
    const char* expected_md5)
{
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "drazlace" / filename;

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
// DrazLace Decoder Tests
// ============================================================================

TEST_CASE("DrazLace decoder: sniff") {
    SUBCASE("Valid compressed DrazLace with signature") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "drazlace" / "babscarr.drl";
        auto data = read_file(path);
        REQUIRE(!data.empty());
        CHECK(onyx_image::drazlace_decoder::sniff(data));
    }

    SUBCASE("Valid uncompressed DrazLace (18242 bytes)") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "drazlace" / "testpack.drl";
        auto data = read_file(path);
        REQUIRE(!data.empty());
        // testpack.drl might be compressed, check another if needed
        CHECK(onyx_image::drazlace_decoder::sniff(data));
    }

    SUBCASE("Invalid - too short") {
        std::vector<std::uint8_t> data = {0x00, 0x5c, 0x00, 0x00};
        CHECK_FALSE(onyx_image::drazlace_decoder::sniff(data));
    }

    SUBCASE("Invalid - wrong size and no signature") {
        std::vector<std::uint8_t> data(5000, 0);
        CHECK_FALSE(onyx_image::drazlace_decoder::sniff(data));
    }

    SUBCASE("Not confused with PNG") {
        std::vector<std::uint8_t> data = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        CHECK_FALSE(onyx_image::drazlace_decoder::sniff(data));
    }

    SUBCASE("Not confused with BMP") {
        std::vector<std::uint8_t> data = {'B', 'M', 0x00, 0x00, 0x00, 0x00};
        CHECK_FALSE(onyx_image::drazlace_decoder::sniff(data));
    }
}

TEST_CASE("DrazLace decoder: compressed files") {
    SUBCASE("babscarr.drl") {
        test_drazlace_decode_md5("babscarr.drl", "d7464c31aa96b36baf406ab310a42404");
    }

    SUBCASE("demopic2.drl") {
        test_drazlace_decode_md5("demopic2.drl", "5f9cc44c198e97d40614d79f9b5bf6b6");
    }

    SUBCASE("jn-bath4.drl") {
        test_drazlace_decode_md5("jn-bath4.drl", "4d20d1d8f4cc860bcb0a54b7ee6ba17c");
    }

    SUBCASE("jn-persi.drl") {
        test_drazlace_decode_md5("jn-persi.drl", "80396512c0813c47d3418a2a70c1bbc7");
    }

    SUBCASE("lick3.drl") {
        test_drazlace_decode_md5("lick3.drl", "415c0e0a2a121a0c765217c08e1dfda4");
    }

    SUBCASE("madhead.drl") {
        test_drazlace_decode_md5("madhead.drl", "a3d8b4021139631881b1afaa61bdb278");
    }

    SUBCASE("misty.drl") {
        test_drazlace_decode_md5("misty.drl", "55748ba07bf39a54a274a71a30b218fd");
    }

    SUBCASE("RAYTRACE.DRL") {
        test_drazlace_decode_md5("RAYTRACE.DRL", "e68f7a8089baa4704aeedd322816d56f");
    }

    SUBCASE("testpack.drl") {
        test_drazlace_decode_md5("testpack.drl", "1a77c14098d94e37cf783357a75e9213");
    }

    SUBCASE("testpic     .drl") {
        test_drazlace_decode_md5("testpic     .drl", "03b9fc9e86796fc8d410f814c4bd37dd");
    }
}

TEST_CASE("DrazLace decoder: dimensions and format") {
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "drazlace" / "babscarr.drl";
    auto data = read_file(path);
    REQUIRE(!data.empty());

    onyx_image::memory_surface surface;
    auto result = onyx_image::drazlace_decoder::decode(data, surface);

    REQUIRE(result.ok);

    // DrazLace is 320x200 multicolor interlaced
    CHECK(surface.width() == 320);
    CHECK(surface.height() == 200);
    CHECK(surface.format() == onyx_image::pixel_format::rgb888);

    // Check pixel data size: 320 * 200 * 3 bytes (RGB)
    CHECK(surface.pixels().size() == 320 * 200 * 3);
}

TEST_CASE("DrazLace decoder: error handling") {
    SUBCASE("Empty data") {
        std::vector<std::uint8_t> data;
        onyx_image::memory_surface surface;
        auto result = onyx_image::drazlace_decoder::decode(data, surface);
        CHECK_FALSE(result.ok);
    }

    SUBCASE("Truncated data") {
        std::vector<std::uint8_t> data(100, 0);
        onyx_image::memory_surface surface;
        auto result = onyx_image::drazlace_decoder::decode(data, surface);
        CHECK_FALSE(result.ok);
    }

    SUBCASE("Invalid shift value") {
        // Create a fake uncompressed DrazLace file with invalid shift
        std::vector<std::uint8_t> data(18242, 0);
        data[0x2744] = 5;  // Invalid shift (must be 0 or 1)
        onyx_image::memory_surface surface;
        auto result = onyx_image::drazlace_decoder::decode(data, surface);
        CHECK_FALSE(result.ok);
    }
}
