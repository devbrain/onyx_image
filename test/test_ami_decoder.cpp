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

void test_ami_decode_md5(
    const char* filename,
    const char* expected_md5)
{
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "ami" / filename;

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
// AMI Decoder Tests
// ============================================================================

TEST_CASE("AMI decoder: sniff") {
    SUBCASE("Valid AMI file with 0x4000 load address") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "ami" / "AIRPORT.AMI";
        auto data = read_file(path);
        REQUIRE(!data.empty());
        CHECK(onyx_image::ami_decoder::sniff(data));
    }

    SUBCASE("Valid AMI file - small compressed") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "ami" / "diskette.ami";
        auto data = read_file(path);
        REQUIRE(!data.empty());
        CHECK(onyx_image::ami_decoder::sniff(data));
    }

    SUBCASE("Invalid - too short") {
        std::vector<std::uint8_t> data = {0x00, 0x40, 0xc2, 0x00};
        CHECK_FALSE(onyx_image::ami_decoder::sniff(data));
    }

    SUBCASE("Invalid - wrong load address (0x6000)") {
        std::vector<std::uint8_t> data(5000, 0);
        data[0] = 0x00;
        data[1] = 0x60;  // 0x6000 load address (GG format)
        CHECK_FALSE(onyx_image::ami_decoder::sniff(data));
    }

    SUBCASE("Invalid - uncompressed size (too large)") {
        std::vector<std::uint8_t> data(10003, 0);
        data[0] = 0x00;
        data[1] = 0x40;
        CHECK_FALSE(onyx_image::ami_decoder::sniff(data));
    }

    SUBCASE("Not confused with PNG") {
        std::vector<std::uint8_t> data = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        CHECK_FALSE(onyx_image::ami_decoder::sniff(data));
    }

    SUBCASE("Not confused with BMP") {
        std::vector<std::uint8_t> data = {'B', 'M', 0x00, 0x00, 0x00, 0x00};
        CHECK_FALSE(onyx_image::ami_decoder::sniff(data));
    }
}

TEST_CASE("AMI decoder: compressed files") {
    SUBCASE("AIRPORT.AMI") {
        test_ami_decode_md5("AIRPORT.AMI", "ad090bedd29254a8bf3fff9d154fef09");
    }

    SUBCASE("64er.ami") {
        test_ami_decode_md5("64er.ami", "239746492a87295899f14960e7b07144");
    }

    SUBCASE("cobra_mk3.ami") {
        test_ami_decode_md5("cobra_mk3.ami", "14ee7156198159507d194c32688b53aa");
    }

    SUBCASE("creator.ami") {
        test_ami_decode_md5("creator.ami", "704d7987a71503bfff3618c89f8d8648");
    }

    SUBCASE("deluxe_kugeln.ami") {
        test_ami_decode_md5("deluxe_kugeln.ami", "59e67b592d49fea7f4146f5b8f08f252");
    }

    SUBCASE("diskette.ami") {
        test_ami_decode_md5("diskette.ami", "95337ff7b48151af7375847b1e1db81b");
    }

    SUBCASE("kugel.ami") {
        test_ami_decode_md5("kugel.ami", "d0aecb95b1357c279ba45f80e39f1c17");
    }

    SUBCASE("london_taxi.ami") {
        test_ami_decode_md5("london_taxi.ami", "39b3d43b51dcb73787070d921d761ae3");
    }

    SUBCASE("miami_vice.ami") {
        test_ami_decode_md5("miami_vice.ami", "c97032bf5d61cdf023bcad46747897dd");
    }

    SUBCASE("screen1.ami") {
        test_ami_decode_md5("screen1.ami", "924ec3df688f2750cd7079ede09c4677");
    }

    SUBCASE("skat.ami") {
        test_ami_decode_md5("skat.ami", "5e1bb8bbc6adfb07c5862d7320692cf0");
    }

    SUBCASE("vulkan.ami") {
        test_ami_decode_md5("vulkan.ami", "fefc4f8ee98b3ad0ae5192894824df56");
    }

    SUBCASE("wald.ami") {
        test_ami_decode_md5("wald.ami", "97b20ab13ea74e4262cfeb6151b4a678");
    }
}

TEST_CASE("AMI decoder: dimensions and format") {
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "ami" / "AIRPORT.AMI";
    auto data = read_file(path);
    REQUIRE(!data.empty());

    onyx_image::memory_surface surface;
    auto result = onyx_image::ami_decoder::decode(data, surface);

    REQUIRE(result.ok);

    // AMI is 320x200 multicolor
    CHECK(surface.width() == 320);
    CHECK(surface.height() == 200);
    CHECK(surface.format() == onyx_image::pixel_format::rgb888);

    // Check pixel data size: 320 * 200 * 3 bytes (RGB)
    CHECK(surface.pixels().size() == 320 * 200 * 3);
}

TEST_CASE("AMI decoder: error handling") {
    SUBCASE("Empty data") {
        std::vector<std::uint8_t> data;
        onyx_image::memory_surface surface;
        auto result = onyx_image::ami_decoder::decode(data, surface);
        CHECK_FALSE(result.ok);
    }

    SUBCASE("Truncated data") {
        std::vector<std::uint8_t> data(50, 0);
        data[0] = 0x00;
        data[1] = 0x40;
        onyx_image::memory_surface surface;
        auto result = onyx_image::ami_decoder::decode(data, surface);
        CHECK_FALSE(result.ok);
    }

    SUBCASE("Dimension limits exceeded") {
        const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "ami" / "AIRPORT.AMI";
        auto data = read_file(path);
        REQUIRE(!data.empty());

        onyx_image::memory_surface surface;
        onyx_image::decode_options opts;
        opts.max_width = 100;  // AMI is 320 wide, so this should fail
        opts.max_height = 100;

        auto result = onyx_image::ami_decoder::decode(data, surface, opts);
        CHECK_FALSE(result.ok);
        CHECK(result.error == onyx_image::decode_error::dimensions_exceeded);
    }
}
