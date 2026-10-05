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

void test_bmp_decode_md5(
    const char* filename,
    const char* expected_md5,
    const char* format_name)
{
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "bmp" / filename;

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

TEST_CASE("BMP decoder: sniff") {
    SUBCASE("Valid BMP signature") {
        std::vector<std::uint8_t> data = {'B', 'M', 0x00, 0x00};
        CHECK(onyx_image::bmp_decoder::sniff(data));
    }

    SUBCASE("Invalid signature") {
        std::vector<std::uint8_t> data = {'P', 'N', 'G', 0x00};
        CHECK_FALSE(onyx_image::bmp_decoder::sniff(data));
    }

    SUBCASE("Too short") {
        std::vector<std::uint8_t> data = {'B'};
        CHECK_FALSE(onyx_image::bmp_decoder::sniff(data));
    }
}

TEST_CASE("BMP decoder: MD5 verification") {
    SUBCASE("Windows 1-bit (test1.bmp)") {
        test_bmp_decode_md5("test1.bmp", "426f3dde0c5a63f25db54bdb861f8e65", "Windows 1-bit");
    }

    SUBCASE("Windows 4-bit (test4.bmp)") {
        test_bmp_decode_md5("test4.bmp", "9c938e73963cb2ed437f3a41bb627e8c", "Windows 4-bit");
    }

    SUBCASE("Windows 8-bit (test8.bmp)") {
        test_bmp_decode_md5("test8.bmp", "7314d3a6f6d7769b212fcbcb329b3bbb", "Windows 8-bit");
    }

    SUBCASE("Windows 16-bit 555 (test16.bmp)") {
        test_bmp_decode_md5("test16.bmp", "db4138ae28c9cbc27995e6a1b7f9a39a", "Windows 16-bit 555");
    }

    SUBCASE("Windows 16-bit 565 bitfields (test16bf565.bmp)") {
        test_bmp_decode_md5("test16bf565.bmp", "a2565f7e3a25fff6c094f93da71ea280", "Windows 16-bit 565");
    }

    SUBCASE("Windows 24-bit (test24.bmp)") {
        test_bmp_decode_md5("test24.bmp", "7b9ef0b6c56392bc095896b013445d7c", "Windows 24-bit");
    }

    SUBCASE("Windows 32-bit (test32.bmp)") {
        test_bmp_decode_md5("test32.bmp", "7b9ef0b6c56392bc095896b013445d7c", "Windows 32-bit");
    }

    SUBCASE("Windows 32-bit bitfields (test32bf.bmp)") {
        test_bmp_decode_md5("test32bf.bmp", "7b9ef0b6c56392bc095896b013445d7c", "Windows 32-bit bitfields");
    }

    SUBCASE("RLE4 compression (testcompress4.bmp)") {
        test_bmp_decode_md5("testcompress4.bmp", "9c938e73963cb2ed437f3a41bb627e8c", "RLE4 compressed");
    }

    SUBCASE("RLE8 compression (testcompress8.bmp)") {
        test_bmp_decode_md5("testcompress8.bmp", "7314d3a6f6d7769b212fcbcb329b3bbb", "RLE8 compressed");
    }

    SUBCASE("OS/2 1.x 8-bit (11Bios13.bmp)") {
        test_bmp_decode_md5("11Bios13.bmp", "be550b483b82121fde3216aad0adf5f8", "OS/2 1.x 8-bit");
    }

    SUBCASE("OS/2 2.x 8-bit (11Bgos20.bmp)") {
        test_bmp_decode_md5("11Bgos20.bmp", "45cc1399f959f39fcb52f4b870e7b721", "OS/2 2.x 8-bit");
    }

    SUBCASE("OS/2 2.x 4-bit (test4os2v2.bmp)") {
        test_bmp_decode_md5("test4os2v2.bmp", "803dbe7a51c73e5ee4b18acacc2c3e32", "OS/2 2.x 4-bit");
    }

    SUBCASE("Windows V4 32-bit (test32bfv4.bmp)") {
        test_bmp_decode_md5("test32bfv4.bmp", "36eb8c0a5144c7a0e22ff0c41736e496", "Windows V4 32-bit");
    }

    SUBCASE("Windows V5 24-bit (test32v5.bmp)") {
        test_bmp_decode_md5("test32v5.bmp", "914d5d4f8f352dbca443a2ba0058c488", "Windows V5 24-bit");
    }
}

namespace {

/// Decodes `filename` with `output` and with color_output::source, and checks that every pixel of the first is
/// the second's color (looked up in its palette when indexed), opaque
void check_expanded_output(const char* filename, onyx_image::color_output output) {
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "bmp" / filename;
    INFO("Testing: ", filename);
    auto data = read_file(path);
    REQUIRE(!data.empty());

    onyx_image::memory_surface source;
    REQUIRE(onyx_image::decode(data, source).ok);

    onyx_image::decode_options options;
    options.output = output;
    onyx_image::memory_surface expanded;
    REQUIRE(onyx_image::decode(data, expanded, options).ok);

    const bool rgba = output == onyx_image::color_output::rgba;
    REQUIRE(expanded.format() == (rgba ? onyx_image::pixel_format::rgba8888 : onyx_image::pixel_format::rgb888));
    REQUIRE(expanded.width() == source.width());
    REQUIRE(expanded.height() == source.height());

    const std::size_t out_bpp = rgba ? 4 : 3;
    const std::size_t src_bpp = onyx_image::bytes_per_pixel(source.format());
    int mismatches = 0;
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            const std::uint8_t* s = source.pixels().data() + static_cast<std::size_t>(y) * source.pitch() +
                                    static_cast<std::size_t>(x) * src_bpp;
            const std::uint8_t* e = expanded.pixels().data() + static_cast<std::size_t>(y) * expanded.pitch() +
                                    static_cast<std::size_t>(x) * out_bpp;
            std::uint8_t r = s[0], g = s[1], b = s[2];
            if (source.format() == onyx_image::pixel_format::indexed8) {
                const std::size_t i = s[0];
                r = source.palette()[i * 3 + 0];
                g = source.palette()[i * 3 + 1];
                b = source.palette()[i * 3 + 2];
            }
            const bool same = e[0] == r && e[1] == g && e[2] == b && (!rgba || e[3] == 0xFF ||
                              (source.format() == onyx_image::pixel_format::rgba8888 && e[3] == s[3]));
            mismatches += same ? 0 : 1;
        }
    }
    CHECK(mismatches == 0);
}

} // namespace

TEST_CASE("BMP decoder: color_output::rgba and ::rgb expand paletted images") {
    for (const char* file : {"test1.bmp", "test4.bmp", "test8.bmp", "testcompress4.bmp", "testcompress8.bmp",
                             "11Bios13.bmp", "test24.bmp", "test32.bmp"}) {
        check_expanded_output(file, onyx_image::color_output::rgba);
        check_expanded_output(file, onyx_image::color_output::rgb);
    }
}
