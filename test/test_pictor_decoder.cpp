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

void test_pictor_decode_md5(
    const char* filename,
    const char* expected_md5,
    const char* format_name)
{
    const std::filesystem::path path = std::filesystem::path(TEST_DATA_DIR) / "pictor" / filename;

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

TEST_CASE("PICTOR decoder: sniff") {
    SUBCASE("Valid PICTOR signature") {
        std::vector<std::uint8_t> data = {0x34, 0x12};
        CHECK(onyx_image::pictor_decoder::sniff(data));
    }

    SUBCASE("Invalid signature") {
        std::vector<std::uint8_t> data = {0x89, 'P', 'N', 'G'};
        CHECK_FALSE(onyx_image::pictor_decoder::sniff(data));
    }

    SUBCASE("Too short") {
        std::vector<std::uint8_t> data = {0x34};
        CHECK_FALSE(onyx_image::pictor_decoder::sniff(data));
    }
}

TEST_CASE("PICTOR decoder: MD5 verification") {
    SUBCASE("VGA 256-color (LEYES.PIC)") {
        test_pictor_decode_md5("LEYES.PIC", "24e1d874a173cc42ac63f70e23912282", "VGA 256-color");
    }

    SUBCASE("VGA 256-color (LGINA.PIC)") {
        test_pictor_decode_md5("LGINA.PIC", "8d6d4891e04513b5b023765c49b60598", "VGA 256-color");
    }

    SUBCASE("EGA 16-color planar (GSAM.PIC)") {
        test_pictor_decode_md5("GSAM.PIC", "46fdb9bd5c493be82b50be9084f00e21", "EGA 16-color planar");
    }

    SUBCASE("EGA 16-color planar (MFISH.PIC)") {
        test_pictor_decode_md5("MFISH.PIC", "04660c98a2cef419b7e38a06c5d41825", "EGA 16-color planar");
    }

    SUBCASE("CGA 4-color (AHOUSE.PIC)") {
        test_pictor_decode_md5("AHOUSE.PIC", "ae9dc18cff4e1e40ee6b8518474f651e", "CGA 4-color");
    }

    SUBCASE("CGA 4-color (ASUNSET.PIC)") {
        test_pictor_decode_md5("ASUNSET.PIC", "1639b056ece36d4f45d6c1d2270714ce", "CGA 4-color");
    }

    SUBCASE("CGA monochrome (CSAM.PIC)") {
        test_pictor_decode_md5("CSAM.PIC", "aeeed010141359cb83a2b21f0b02caba", "CGA monochrome");
    }

    SUBCASE("EGA monochrome (EMOUSE.PIC)") {
        test_pictor_decode_md5("EMOUSE.PIC", "ecaf74d375f263c74cf62094d85109cc", "EGA monochrome");
    }

    SUBCASE("Monochrome (OPOODLE.PIC)") {
        test_pictor_decode_md5("OPOODLE.PIC", "82889ec698684ca070832c83e998b98e", "Monochrome");
    }

    SUBCASE("Monochrome (OWALDO.PIC)") {
        test_pictor_decode_md5("OWALDO.PIC", "6c46b361471493af18b57c496f098d9d", "Monochrome");
    }
}
