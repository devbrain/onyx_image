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

void test_msp_decode_md5(
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
    CHECK(surface.format() == onyx_image::pixel_format::indexed8);

    std::string actual_md5 = compute_surface_md5(surface);
    CHECK(actual_md5 == expected_md5);
}

} // namespace

TEST_CASE("MSP decoder: sniff") {
    SUBCASE("Valid MSP v1 magic (DanM)") {
        // MSP v1 magic: 0x6144, 0x4D6E (little-endian)
        std::vector<std::uint8_t> data = {0x44, 0x61, 0x6E, 0x4D};
        CHECK(onyx_image::msp_decoder::sniff(data));
    }

    SUBCASE("Valid MSP v2 magic (LinS)") {
        // MSP v2 magic: 0x694C, 0x536E (little-endian)
        std::vector<std::uint8_t> data = {0x4C, 0x69, 0x6E, 0x53};
        CHECK(onyx_image::msp_decoder::sniff(data));
    }

    SUBCASE("Invalid - wrong magic") {
        std::vector<std::uint8_t> data = {0x00, 0x00, 0x00, 0x00};
        CHECK_FALSE(onyx_image::msp_decoder::sniff(data));
    }

    SUBCASE("Invalid - too short") {
        std::vector<std::uint8_t> data = {0x44, 0x61, 0x6E};
        CHECK_FALSE(onyx_image::msp_decoder::sniff(data));
    }

    SUBCASE("Not confused with PCX") {
        // PCX magic is 0x0A
        std::vector<std::uint8_t> data = {0x0A, 0x05, 0x01, 0x08};
        CHECK_FALSE(onyx_image::msp_decoder::sniff(data));
    }
}

TEST_CASE("MSP decoder: MD5 verification") {
    // hopper.msp is a 128x128 1-bit monochrome image (version 1, uncompressed)
    test_msp_decode_md5("msp/hopper.msp", "860202c427401ef526bbf8b8ae7be22e", 128, 128);
}
