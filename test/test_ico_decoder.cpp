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

void test_ico_decode_md5(
    const char* filename,
    const char* expected_md5,
    int expected_width,
    int expected_height)
{
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / filename;

    INFO("Testing: ", filename);
    REQUIRE(std::filesystem::exists(path));

    auto data = read_file(path);
    REQUIRE(!data.empty());

    onyx_image::memory_surface surface;
    auto result = onyx_image::decode(data, surface);

    REQUIRE(result.ok);
    CHECK(surface.width() == expected_width);
    CHECK(surface.height() == expected_height);

    std::string actual_md5 = compute_surface_md5(surface);
    CHECK(actual_md5 == expected_md5);
}

} // namespace

// ============================================================================
// ICO Decoder Tests
// ============================================================================

TEST_CASE("ICO decoder: sniff") {
    SUBCASE("Valid ICO file") {
        // ICO header: reserved=0, type=1 (icon), count=1
        std::vector<std::uint8_t> data = {0x00, 0x00, 0x01, 0x00, 0x01, 0x00};
        CHECK(onyx_image::ico_decoder::sniff(data));
    }

    SUBCASE("Valid CUR file") {
        // CUR header: reserved=0, type=2 (cursor), count=1
        std::vector<std::uint8_t> data = {0x00, 0x00, 0x02, 0x00, 0x01, 0x00};
        CHECK(onyx_image::ico_decoder::sniff(data));
    }

    SUBCASE("Invalid - wrong reserved field") {
        std::vector<std::uint8_t> data = {0x01, 0x00, 0x01, 0x00, 0x01, 0x00};
        CHECK_FALSE(onyx_image::ico_decoder::sniff(data));
    }

    SUBCASE("Invalid - wrong type") {
        std::vector<std::uint8_t> data = {0x00, 0x00, 0x03, 0x00, 0x01, 0x00};
        CHECK_FALSE(onyx_image::ico_decoder::sniff(data));
    }

    SUBCASE("Invalid - zero count") {
        std::vector<std::uint8_t> data = {0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
        CHECK_FALSE(onyx_image::ico_decoder::sniff(data));
    }

    SUBCASE("Invalid - too short") {
        std::vector<std::uint8_t> data = {0x00, 0x00, 0x01, 0x00, 0x01};
        CHECK_FALSE(onyx_image::ico_decoder::sniff(data));
    }

    SUBCASE("Not confused with BMP") {
        std::vector<std::uint8_t> data = {'B', 'M', 0x00, 0x00, 0x00, 0x00};
        CHECK_FALSE(onyx_image::ico_decoder::sniff(data));
    }

    SUBCASE("Not confused with PNG") {
        std::vector<std::uint8_t> data = {0x89, 'P', 'N', 'G', 0x0D, 0x0A};
        CHECK_FALSE(onyx_image::ico_decoder::sniff(data));
    }
}

TEST_CASE("ICO decoder: single icon") {
    // hopper.ico: 16x16 8-bit icon
    test_ico_decode_md5("Pillow/Tests/images/hopper.ico",
                        "5a7682b8322cc5801686c2935ed9f47b",
                        16, 16);
}

TEST_CASE("ICO decoder: multi-size icon atlas") {
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) /
                                        "Pillow/Tests/images/python.ico";

    REQUIRE(std::filesystem::exists(path));

    auto data = read_file(path);
    REQUIRE(!data.empty());

    onyx_image::memory_surface surface;
    auto result = onyx_image::decode(data, surface);

    REQUIRE(result.ok);

    // python.ico has 3 icons: 16x16, 32x32, 48x48 stacked vertically
    CHECK(surface.width() == 48);  // Max width
    CHECK(surface.height() == 96); // 16 + 32 + 48

    // Check subrects
    const auto& subrects = surface.subrects();
    REQUIRE(subrects.size() == 3);

    // First icon: 16x16
    CHECK(subrects[0].rect.x == 0);
    CHECK(subrects[0].rect.y == 0);
    CHECK(subrects[0].rect.w == 16);
    CHECK(subrects[0].rect.h == 16);
    CHECK(subrects[0].kind == onyx_image::subrect_kind::sprite);
    CHECK(subrects[0].user_tag == 0);

    // Second icon: 32x32
    CHECK(subrects[1].rect.x == 0);
    CHECK(subrects[1].rect.y == 16);
    CHECK(subrects[1].rect.w == 32);
    CHECK(subrects[1].rect.h == 32);
    CHECK(subrects[1].kind == onyx_image::subrect_kind::sprite);
    CHECK(subrects[1].user_tag == 1);

    // Third icon: 48x48
    CHECK(subrects[2].rect.x == 0);
    CHECK(subrects[2].rect.y == 48);
    CHECK(subrects[2].rect.w == 48);
    CHECK(subrects[2].rect.h == 48);
    CHECK(subrects[2].kind == onyx_image::subrect_kind::sprite);
    CHECK(subrects[2].user_tag == 2);
}

TEST_CASE("ICO decoder: PNG-compressed icon") {
    // hopper_256x256.ico uses PNG compression (Vista+ format)
    test_ico_decode_md5("Pillow/Tests/images/hopper_256x256.ico",
                        "6daffd5161ae22e4804064fa9ea82d7b",
                        256, 256);
}

// ============================================================================
// EXE Icon Decoder Tests
// ============================================================================

TEST_CASE("EXE icon decoder: sniff") {
    SUBCASE("Valid MZ signature") {
        // MZ header followed by NE/PE would be detected
        // But just MZ alone returns false (need NE/PE/LX)
        std::vector<std::uint8_t> data(64, 0);
        data[0] = 'M';
        data[1] = 'Z';
        // Without proper NE/PE/LX header, should return false
        CHECK_FALSE(onyx_image::exe_icon_decoder::sniff(data));
    }

    SUBCASE("Invalid - wrong signature") {
        std::vector<std::uint8_t> data(64, 0);
        data[0] = 'P';
        data[1] = 'K';
        CHECK_FALSE(onyx_image::exe_icon_decoder::sniff(data));
    }

    SUBCASE("Invalid - too short") {
        std::vector<std::uint8_t> data = {'M', 'Z'};
        CHECK_FALSE(onyx_image::exe_icon_decoder::sniff(data));
    }

    SUBCASE("Not confused with ICO") {
        std::vector<std::uint8_t> data = {0x00, 0x00, 0x01, 0x00, 0x01, 0x00};
        CHECK_FALSE(onyx_image::exe_icon_decoder::sniff(data));
    }
}

TEST_CASE("EXE icon decoder: NE executable") {
    // PROGMAN.EXE from Windows 3.11 - NE format with 92 icons
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "PROGMAN.EXE";

    REQUIRE(std::filesystem::exists(path));

    auto data = read_file(path);
    REQUIRE(!data.empty());

    // Verify sniff detects it as exe_icon
    CHECK(onyx_image::exe_icon_decoder::sniff(data));
    CHECK_FALSE(onyx_image::ico_decoder::sniff(data));

    onyx_image::memory_surface surface;
    auto result = onyx_image::decode(data, surface);

    REQUIRE(result.ok);

    // PROGMAN.EXE has 92 32x32 icons
    CHECK(surface.width() == 32);
    CHECK(surface.height() == 2944);  // 92 * 32

    // Check we have subrects for all icons
    const auto& subrects = surface.subrects();
    CHECK(subrects.size() == 92);

    // Verify first and last subrect
    if (!subrects.empty()) {
        CHECK(subrects[0].rect.x == 0);
        CHECK(subrects[0].rect.y == 0);
        CHECK(subrects[0].rect.w == 32);
        CHECK(subrects[0].rect.h == 32);
        CHECK(subrects[0].kind == onyx_image::subrect_kind::sprite);
        CHECK(subrects[0].user_tag == 0);
    }

    if (subrects.size() >= 92) {
        CHECK(subrects[91].rect.y == 91 * 32);
        CHECK(subrects[91].user_tag == 91);
    }
}
