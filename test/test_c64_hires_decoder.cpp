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

void test_c64_hires_decode_md5(
    const char* filename,
    const char* expected_md5)
{
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "c64hires" / filename;

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
// C64 Hires Decoder Tests
// ============================================================================

TEST_CASE("C64 hires decoder: sniff") {
    SUBCASE("Valid C64 hires file (.hbm)") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "c64hires" / "test.hbm";
        auto data = read_file(path);
        REQUIRE(!data.empty());
        CHECK(onyx_image::c64_hires_decoder::sniff(data));
    }

    SUBCASE("Valid C64 hires file (.fgs)") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "c64hires" / "test.fgs";
        auto data = read_file(path);
        REQUIRE(!data.empty());
        CHECK(onyx_image::c64_hires_decoder::sniff(data));
    }

    SUBCASE("Valid C64 hires file (.gih)") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "c64hires" / "GFUCHS.gih";
        auto data = read_file(path);
        REQUIRE(!data.empty());
        CHECK(onyx_image::c64_hires_decoder::sniff(data));
    }

    SUBCASE("Invalid - too short") {
        std::vector<std::uint8_t> data = {0x00, 0x20};
        CHECK_FALSE(onyx_image::c64_hires_decoder::sniff(data));
    }

    SUBCASE("Invalid - wrong size") {
        std::vector<std::uint8_t> data(1000, 0);
        data[0] = 0x00;
        data[1] = 0x20;  // Valid load address
        CHECK_FALSE(onyx_image::c64_hires_decoder::sniff(data));
    }

    SUBCASE("Invalid - wrong load address") {
        std::vector<std::uint8_t> data(8002, 0);
        data[0] = 0x00;
        data[1] = 0x01;  // Invalid load address
        CHECK_FALSE(onyx_image::c64_hires_decoder::sniff(data));
    }

    SUBCASE("Not confused with PNG") {
        std::vector<std::uint8_t> data = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        CHECK_FALSE(onyx_image::c64_hires_decoder::sniff(data));
    }

    SUBCASE("Not confused with BMP") {
        std::vector<std::uint8_t> data = {'B', 'M', 0x00, 0x00, 0x00, 0x00};
        CHECK_FALSE(onyx_image::c64_hires_decoder::sniff(data));
    }
}

TEST_CASE("C64 hires decoder: .hbm files") {
    SUBCASE("test.hbm") {
        test_c64_hires_decode_md5("test.hbm", "5446f9f6e0be736c65a08c1becb3aa19");
    }

    SUBCASE("AMBER.HBM") {
        test_c64_hires_decode_md5("AMBER.HBM", "e543d17097087e2faf4b614576c96f10");
    }
}

TEST_CASE("C64 hires decoder: .fgs files") {
    SUBCASE("test.fgs") {
        test_c64_hires_decode_md5("test.fgs", "4ab76926c52c811c275da46bb10a36ba");
    }
}

TEST_CASE("C64 hires decoder: .gih files") {
    SUBCASE("GFUCHS.gih") {
        test_c64_hires_decode_md5("GFUCHS.gih", "ff130074f97b8887c33e393e2dd4e62b");
    }
}

TEST_CASE("C64 hires decoder: other extensions") {
    SUBCASE("Camera (no extension)") {
        test_c64_hires_decode_md5("Camera", "c575b0992fda02d55f333ec85b193749");
    }

    SUBCASE("diane.c64") {
        test_c64_hires_decode_md5("diane.c64", "d31dacf7826b76aedd1145412bef8527");
    }

    SUBCASE("dogs_girl (no extension)") {
        test_c64_hires_decode_md5("dogs_girl", "29236fbed863288d5133e96b9b4a64e6");
    }

    SUBCASE("dragon.d") {
        test_c64_hires_decode_md5("dragon.d", "c3b88ea76f85e52a2b3aaa7a7a86c69b");
    }

    SUBCASE("niemanazwy-bimber.hpi") {
        test_c64_hires_decode_md5("niemanazwy-bimber.hpi", "8bc6b72943d73251c7b0151587628197");
    }
}

TEST_CASE("C64 hires decoder: dimensions and format") {
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "c64hires" / "test.hbm";
    auto data = read_file(path);
    REQUIRE(!data.empty());

    onyx_image::memory_surface surface;
    auto result = onyx_image::c64_hires_decoder::decode(data, surface);

    REQUIRE(result.ok);

    // C64 hires is 320x200
    CHECK(surface.width() == 320);
    CHECK(surface.height() == 200);
    CHECK(surface.format() == onyx_image::pixel_format::rgb888);

    // Check pixel data size: 320 * 200 * 3 bytes (RGB)
    CHECK(surface.pixels().size() == 320 * 200 * 3);
}

TEST_CASE("C64 hires decoder: error handling") {
    SUBCASE("Empty data") {
        std::vector<std::uint8_t> data;
        onyx_image::memory_surface surface;
        auto result = onyx_image::c64_hires_decoder::decode(data, surface);
        CHECK_FALSE(result.ok);
    }

    SUBCASE("Invalid file size") {
        std::vector<std::uint8_t> data(1000, 0);
        data[0] = 0x00;
        data[1] = 0x20;
        onyx_image::memory_surface surface;
        auto result = onyx_image::c64_hires_decoder::decode(data, surface);
        CHECK_FALSE(result.ok);
    }

    SUBCASE("Dimension limits exceeded") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "c64hires" / "test.hbm";
        auto data = read_file(path);
        REQUIRE(!data.empty());

        onyx_image::memory_surface surface;
        onyx_image::decode_options opts;
        opts.max_width = 100;  // C64 hires is 320 wide, so this should fail
        opts.max_height = 100;

        auto result = onyx_image::c64_hires_decoder::decode(data, surface, opts);
        CHECK_FALSE(result.ok);
        CHECK(result.error == onyx_image::decode_error::dimensions_exceeded);
    }
}
