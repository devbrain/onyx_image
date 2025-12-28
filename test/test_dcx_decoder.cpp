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

void test_dcx_decode_md5(
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

TEST_CASE("DCX decoder: sniff") {
    SUBCASE("Valid DCX magic") {
        // DCX magic is 0x3ADE68B1 in little-endian
        std::vector<std::uint8_t> data = {0xB1, 0x68, 0xDE, 0x3A};
        CHECK(onyx_image::dcx_decoder::sniff(data));
    }

    SUBCASE("Invalid - wrong magic") {
        std::vector<std::uint8_t> data = {0x00, 0x00, 0x00, 0x00};
        CHECK_FALSE(onyx_image::dcx_decoder::sniff(data));
    }

    SUBCASE("Invalid - too short") {
        std::vector<std::uint8_t> data = {0xB1, 0x68, 0xDE};
        CHECK_FALSE(onyx_image::dcx_decoder::sniff(data));
    }

    SUBCASE("Not confused with PCX") {
        // PCX magic is 0x0A
        std::vector<std::uint8_t> data = {0x0A, 0x05, 0x01, 0x08};
        CHECK_FALSE(onyx_image::dcx_decoder::sniff(data));
    }
}

TEST_CASE("DCX decoder: MD5 verification") {
    // hopper.dcx is a 128x128 8-bit RGB image in DCX container
    test_dcx_decode_md5("dcx/hopper.dcx", "963993a4bde036e6ad97ed553d45b359", 128, 128);
}

TEST_CASE("DCX decoder: multi-page atlas") {
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "dcx/multipage.dcx";

    REQUIRE(std::filesystem::exists(path));

    auto data = read_file(path);
    REQUIRE(!data.empty());

    onyx_image::memory_surface surface;
    auto result = onyx_image::decode(data, surface);

    REQUIRE(result.ok);

    // 3 pages of 128x128, stacked vertically
    CHECK(surface.width() == 128);
    CHECK(surface.height() == 384);

    // Check subrects
    const auto& subrects = surface.subrects();
    REQUIRE(subrects.size() == 3);

    CHECK(subrects[0].rect.x == 0);
    CHECK(subrects[0].rect.y == 0);
    CHECK(subrects[0].rect.w == 128);
    CHECK(subrects[0].rect.h == 128);
    CHECK(subrects[0].kind == onyx_image::subrect_kind::frame);
    CHECK(subrects[0].user_tag == 0);

    CHECK(subrects[1].rect.x == 0);
    CHECK(subrects[1].rect.y == 128);
    CHECK(subrects[1].rect.w == 128);
    CHECK(subrects[1].rect.h == 128);
    CHECK(subrects[1].kind == onyx_image::subrect_kind::frame);
    CHECK(subrects[1].user_tag == 1);

    CHECK(subrects[2].rect.x == 0);
    CHECK(subrects[2].rect.y == 256);
    CHECK(subrects[2].rect.w == 128);
    CHECK(subrects[2].rect.h == 128);
    CHECK(subrects[2].kind == onyx_image::subrect_kind::frame);
    CHECK(subrects[2].user_tag == 2);
}
