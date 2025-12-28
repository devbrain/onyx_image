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

void test_pnm_decode_md5(
    const char* filename,
    const char* expected_md5,
    const char* format_name)
{
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / filename;

    INFO("Testing: ", filename, " (", format_name, ")");
    REQUIRE(std::filesystem::exists(path));

    auto data = read_file(path);
    REQUIRE(!data.empty());

    onyx_image::memory_surface surface;
    auto result = onyx_image::decode(data, surface);

    REQUIRE(result.ok);
    REQUIRE(surface.width() > 0);
    REQUIRE(surface.height() > 0);

    std::string actual_md5 = compute_surface_md5(surface);
    CHECK(actual_md5 == expected_md5);
}

} // namespace

TEST_CASE("PNM decoder: sniff") {
    SUBCASE("Valid P1 (ASCII PBM)") {
        std::vector<std::uint8_t> data = {'P', '1', '\n'};
        CHECK(onyx_image::pnm_decoder::sniff(data));
    }

    SUBCASE("Valid P6 (Binary PPM)") {
        std::vector<std::uint8_t> data = {'P', '6', ' '};
        CHECK(onyx_image::pnm_decoder::sniff(data));
    }

    SUBCASE("Invalid - not P") {
        std::vector<std::uint8_t> data = {'X', '6', '\n'};
        CHECK_FALSE(onyx_image::pnm_decoder::sniff(data));
    }

    SUBCASE("Invalid - P7 not supported") {
        std::vector<std::uint8_t> data = {'P', '7', '\n'};
        CHECK_FALSE(onyx_image::pnm_decoder::sniff(data));
    }

    SUBCASE("Invalid - no whitespace") {
        std::vector<std::uint8_t> data = {'P', '6', '1'};
        CHECK_FALSE(onyx_image::pnm_decoder::sniff(data));
    }

    SUBCASE("Too short") {
        std::vector<std::uint8_t> data = {'P', '6'};
        CHECK_FALSE(onyx_image::pnm_decoder::sniff(data));
    }
}

TEST_CASE("PNM decoder: MD5 verification") {
    SUBCASE("P1 - ASCII PBM (1-bit bitmap)") {
        test_pnm_decode_md5("pnm/hopper_1bit_plain.pbm", "3050b338b1d5ad8acb9b860affd54afb", "ASCII PBM");
    }

    SUBCASE("P4 - Binary PBM (1-bit bitmap)") {
        test_pnm_decode_md5("pnm/hopper_1bit.pbm", "3050b338b1d5ad8acb9b860affd54afb", "Binary PBM");
    }

    SUBCASE("P2 - ASCII PGM (8-bit grayscale)") {
        test_pnm_decode_md5("pnm/hopper_8bit_plain.pgm", "ef21d7573f29014382e10bf2f53c3ea2", "ASCII PGM");
    }

    SUBCASE("P5 - Binary PGM (8-bit grayscale)") {
        test_pnm_decode_md5("pnm/hopper_8bit.pgm", "ef21d7573f29014382e10bf2f53c3ea2", "Binary PGM");
    }

    SUBCASE("P5 - Binary PGM (16-bit grayscale)") {
        test_pnm_decode_md5("pnm/16_bit_binary.pgm", "fa48ab8aee94adc8ce5a6906a8c37edf", "Binary PGM 16-bit");
    }

    SUBCASE("P3 - ASCII PPM (RGB)") {
        test_pnm_decode_md5("pnm/hopper_8bit_plain.ppm", "e0902075a2396bb3a58873b3fbf259bd", "ASCII PPM");
    }

    SUBCASE("P6 - Binary PPM (RGB)") {
        test_pnm_decode_md5("pnm/hopper.ppm", "963993a4bde036e6ad97ed553d45b359", "Binary PPM");
    }
}
