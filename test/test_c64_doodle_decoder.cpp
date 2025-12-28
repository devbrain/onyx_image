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

void test_c64_doodle_decode_md5(
    const char* filename,
    const char* expected_md5)
{
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "c64_doodle" / filename;

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
// C64 Doodle Decoder Tests
// ============================================================================

TEST_CASE("C64 Doodle decoder: sniff") {
    SUBCASE("Valid uncompressed Doodle (9218 bytes)") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "c64_doodle" / "abydos.dd";
        auto data = read_file(path);
        REQUIRE(!data.empty());
        CHECK(onyx_image::c64_doodle_decoder::sniff(data));
    }

    SUBCASE("Valid uncompressed Doodle (9026 bytes - Run Paint)") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "c64_doodle" / "DDC64 COMPUTER";
        auto data = read_file(path);
        REQUIRE(!data.empty());
        CHECK(onyx_image::c64_doodle_decoder::sniff(data));
    }

    SUBCASE("Valid JJ compressed Doodle") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "c64_doodle" / "JJMACROSS.JJ";
        auto data = read_file(path);
        REQUIRE(!data.empty());
        CHECK(onyx_image::c64_doodle_decoder::sniff(data));
    }

    SUBCASE("Valid JJ compressed Doodle (extended variant)") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "c64_doodle" / "godot.JJ";
        auto data = read_file(path);
        REQUIRE(!data.empty());
        CHECK(onyx_image::c64_doodle_decoder::sniff(data));
    }

    SUBCASE("Invalid - too short") {
        std::vector<std::uint8_t> data = {0x00, 0x5c, 0x00, 0x00};
        CHECK_FALSE(onyx_image::c64_doodle_decoder::sniff(data));
    }

    SUBCASE("Invalid - wrong size") {
        std::vector<std::uint8_t> data(5000, 0);
        CHECK_FALSE(onyx_image::c64_doodle_decoder::sniff(data));
    }

    SUBCASE("Not confused with PNG") {
        std::vector<std::uint8_t> data = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        CHECK_FALSE(onyx_image::c64_doodle_decoder::sniff(data));
    }

    SUBCASE("Not confused with BMP") {
        std::vector<std::uint8_t> data = {'B', 'M', 0x00, 0x00, 0x00, 0x00};
        CHECK_FALSE(onyx_image::c64_doodle_decoder::sniff(data));
    }
}

TEST_CASE("C64 Doodle decoder: uncompressed DD files (9218 bytes)") {
    SUBCASE("abydos.dd") {
        test_c64_doodle_decode_md5("abydos.dd", "2f4c9de35a68c3f9c832ec44aa568185");
    }

    SUBCASE("eldiva.dd") {
        test_c64_doodle_decode_md5("eldiva.dd", "097c9cba3db7dbe67e34486e1e244ca8");
    }

    SUBCASE("midear.dd") {
        test_c64_doodle_decode_md5("midear.dd", "da778b997710d220b72361789dbdc16f");
    }

    SUBCASE("natalie.dd") {
        test_c64_doodle_decode_md5("natalie.dd", "9e87ea595fe14d9dd73a8ca3680d4633");
    }
}

TEST_CASE("C64 Doodle decoder: Run Paint files (9026 bytes)") {
    SUBCASE("DDC64 COMPUTER") {
        test_c64_doodle_decode_md5("DDC64 COMPUTER", "8cf81829cd48959bfd45552a230bfb5d");
    }

    SUBCASE("DDDIRTY PAIR") {
        test_c64_doodle_decode_md5("DDDIRTY PAIR", "738f70a5cd6fd9bba359542a5019ffa3");
    }

    SUBCASE("DDJAPANESE GIRL") {
        test_c64_doodle_decode_md5("DDJAPANESE GIRL", "962661e927b6eb05aef9b1ab7d6a379f");
    }

    SUBCASE("DDLIL GAL") {
        test_c64_doodle_decode_md5("DDLIL GAL", "cb1bc3f9b39b9af4acf4d58f94d11185");
    }
}

TEST_CASE("C64 Doodle decoder: JJ compressed files") {
    SUBCASE("JJMACROSS.JJ") {
        test_c64_doodle_decode_md5("JJMACROSS.JJ", "a6e1266ee51578abe4058ddb501d20c5");
    }

    SUBCASE("godot.JJ (extended variant)") {
        test_c64_doodle_decode_md5("godot.JJ", "4bc4c16c22187d67c3be0bcce0af8d16");
    }
}

TEST_CASE("C64 Doodle decoder: dimensions and format") {
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "c64_doodle" / "abydos.dd";
    auto data = read_file(path);
    REQUIRE(!data.empty());

    onyx_image::memory_surface surface;
    auto result = onyx_image::c64_doodle_decoder::decode(data, surface);

    REQUIRE(result.ok);

    // C64 hires mode is 320x200 at 1:1 aspect ratio
    CHECK(surface.width() == 320);
    CHECK(surface.height() == 200);
    CHECK(surface.format() == onyx_image::pixel_format::rgb888);

    // Check pixel data size: 320 * 200 * 3 bytes (RGB)
    CHECK(surface.pixels().size() == 320 * 200 * 3);
}

TEST_CASE("C64 Doodle decoder: error handling") {
    SUBCASE("Empty data") {
        std::vector<std::uint8_t> data;
        onyx_image::memory_surface surface;
        auto result = onyx_image::c64_doodle_decoder::decode(data, surface);
        CHECK_FALSE(result.ok);
    }

    SUBCASE("Truncated data") {
        std::vector<std::uint8_t> data(100, 0);
        onyx_image::memory_surface surface;
        auto result = onyx_image::c64_doodle_decoder::decode(data, surface);
        CHECK_FALSE(result.ok);
    }
}
