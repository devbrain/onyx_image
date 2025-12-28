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

void test_atarist_decode_md5(
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

TEST_CASE("NEO decoder: sniff") {
    SUBCASE("Valid NEO file size and header") {
        // NEO files are exactly 32128 bytes
        std::vector<std::uint8_t> data(32128, 0);
        // First 4 bytes: 00 00 00 00 (flag=0, resolution=0)
        CHECK(onyx_image::neo_decoder::sniff(data));
    }

    SUBCASE("Invalid - wrong size") {
        std::vector<std::uint8_t> data(32000, 0);
        CHECK_FALSE(onyx_image::neo_decoder::sniff(data));
    }

    SUBCASE("Invalid - non-zero flag") {
        std::vector<std::uint8_t> data(32128, 0);
        data[0] = 0x01;
        CHECK_FALSE(onyx_image::neo_decoder::sniff(data));
    }

    SUBCASE("Invalid - bad resolution") {
        std::vector<std::uint8_t> data(32128, 0);
        data[2] = 0x00;
        data[3] = 0x05; // Resolution 5 is invalid
        CHECK_FALSE(onyx_image::neo_decoder::sniff(data));
    }
}

TEST_CASE("DEGAS decoder: sniff") {
    SUBCASE("Valid uncompressed DEGAS (standard)") {
        std::vector<std::uint8_t> data(32034, 0);
        // Resolution 0 (low-res)
        CHECK(onyx_image::degas_decoder::sniff(data));
    }

    SUBCASE("Valid uncompressed DEGAS Elite") {
        std::vector<std::uint8_t> data(32066, 0);
        CHECK(onyx_image::degas_decoder::sniff(data));
    }

    SUBCASE("Valid compressed DEGAS") {
        std::vector<std::uint8_t> data(1000, 0);
        data[0] = 0x80; // Compressed marker
        data[1] = 0x00; // Resolution 0
        CHECK(onyx_image::degas_decoder::sniff(data));
    }

    SUBCASE("Invalid - wrong size for uncompressed") {
        std::vector<std::uint8_t> data(30000, 0);
        CHECK_FALSE(onyx_image::degas_decoder::sniff(data));
    }
}

TEST_CASE("Atari ST decoders: MD5 verification") {
    SUBCASE("NEO - low resolution") {
        // MEDUSABL.NEO is 320x200, 16 colors
        test_atarist_decode_md5("atarist/MEDUSABL.NEO", "d9e5706fe74ade547b04a7a72335e5f7", 320, 200);
    }

    SUBCASE("DEGAS uncompressed - low resolution") {
        // LOWRES.PI1 is 320x200, 16 colors
        test_atarist_decode_md5("atarist/LOWRES.PI1", "a92db2fc3c5328e79aca489874b044fb", 320, 200);
    }

    SUBCASE("DEGAS compressed - low resolution") {
        // LOWRES.PC1 is 320x200, 16 colors (compressed)
        // Should match LOWRES.PI1 (same image, different compression)
        test_atarist_decode_md5("atarist/LOWRES.PC1", "a92db2fc3c5328e79aca489874b044fb", 320, 200);
    }
}
