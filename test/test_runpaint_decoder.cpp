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

void test_runpaint_decode_md5(
    const char* filename,
    const char* expected_md5)
{
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "runpaint" / filename;

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
// Run Paint Decoder Tests
// ============================================================================

TEST_CASE("Run Paint decoder: sniff") {
    SUBCASE("Valid Run Paint file (.rpm)") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "runpaint" / "abydos.rpm";
        auto data = read_file(path);
        REQUIRE(!data.empty());
        CHECK(onyx_image::runpaint_decoder::sniff(data));
    }

    SUBCASE("Valid Run Paint file (10006 bytes)") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "runpaint" / "STILLIFE.rpm";
        auto data = read_file(path);
        REQUIRE(!data.empty());
        CHECK(onyx_image::runpaint_decoder::sniff(data));
    }

    SUBCASE("Invalid - too short") {
        std::vector<std::uint8_t> data = {0x00, 0x60};
        CHECK_FALSE(onyx_image::runpaint_decoder::sniff(data));
    }

    SUBCASE("Invalid - wrong size") {
        std::vector<std::uint8_t> data(1000, 0);
        data[0] = 0x00;
        data[1] = 0x60;  // Valid load address
        CHECK_FALSE(onyx_image::runpaint_decoder::sniff(data));
    }

    SUBCASE("Invalid - wrong load address") {
        std::vector<std::uint8_t> data(10003, 0);
        data[0] = 0x00;
        data[1] = 0x01;  // Invalid load address
        CHECK_FALSE(onyx_image::runpaint_decoder::sniff(data));
    }

    SUBCASE("Not confused with PNG") {
        std::vector<std::uint8_t> data = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        CHECK_FALSE(onyx_image::runpaint_decoder::sniff(data));
    }
}

TEST_CASE("Run Paint decoder: .rpm files (10003 bytes)") {
    SUBCASE("abydos.rpm") {
        test_runpaint_decode_md5("abydos.rpm", "27441fa1005e0dbd8a6a33302424d02a");
    }

    SUBCASE("coaster.rpm") {
        test_runpaint_decode_md5("coaster.rpm", "da3ee2893ece087507fc86e53fc552b0");
    }

    SUBCASE("photon.eye.rpm") {
        test_runpaint_decode_md5("photon.eye.rpm", "d23f4591a8911a6b6e1d9423c3ccc3e5");
    }

    SUBCASE("still.life.rpm") {
        test_runpaint_decode_md5("still.life.rpm", "31c8fd70e0075fd3d7be4e0136719662");
    }
}

TEST_CASE("Run Paint decoder: .rpm files (10006 bytes)") {
    SUBCASE("STILLIFE.rpm") {
        test_runpaint_decode_md5("STILLIFE.rpm", "31c8fd70e0075fd3d7be4e0136719662");
    }
}

TEST_CASE("Run Paint decoder: files without extension") {
    SUBCASE("gol") {
        test_runpaint_decode_md5("gol", "5ee66a71e5eee2c75fe0e90a895d630d");
    }

    SUBCASE("kom") {
        test_runpaint_decode_md5("kom", "d4bf07f006bb99b479476a55eb1bd532");
    }

    SUBCASE("Antic") {
        test_runpaint_decode_md5("Antic", "20fec53113297e76ff04636861eea072");
    }
}

TEST_CASE("Run Paint decoder: dimensions and format") {
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "runpaint" / "abydos.rpm";
    auto data = read_file(path);
    REQUIRE(!data.empty());

    onyx_image::memory_surface surface;
    auto result = onyx_image::runpaint_decoder::decode(data, surface);

    REQUIRE(result.ok);

    // Run Paint is 320x200 (C64 multicolor)
    CHECK(surface.width() == 320);
    CHECK(surface.height() == 200);
    CHECK(surface.format() == onyx_image::pixel_format::rgb888);

    // Check pixel data size: 320 * 200 * 3 bytes (RGB)
    CHECK(surface.pixels().size() == 320 * 200 * 3);
}

TEST_CASE("Run Paint decoder: error handling") {
    SUBCASE("Empty data") {
        std::vector<std::uint8_t> data;
        onyx_image::memory_surface surface;
        auto result = onyx_image::runpaint_decoder::decode(data, surface);
        CHECK_FALSE(result.ok);
    }

    SUBCASE("Invalid file size") {
        std::vector<std::uint8_t> data(1000, 0);
        data[0] = 0x00;
        data[1] = 0x60;
        onyx_image::memory_surface surface;
        auto result = onyx_image::runpaint_decoder::decode(data, surface);
        CHECK_FALSE(result.ok);
    }

    SUBCASE("Dimension limits exceeded") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "runpaint" / "abydos.rpm";
        auto data = read_file(path);
        REQUIRE(!data.empty());

        onyx_image::memory_surface surface;
        onyx_image::decode_options opts;
        opts.max_width = 100;  // Run Paint is 320 wide, so this should fail
        opts.max_height = 100;

        auto result = onyx_image::runpaint_decoder::decode(data, surface, opts);
        CHECK_FALSE(result.ok);
        CHECK(result.error == onyx_image::decode_error::dimensions_exceeded);
    }
}
