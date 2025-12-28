#include <onyx_image/onyx_image.hpp>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

void print_usage(const char* program) {
    std::cerr << "Usage: " << program << " [options] <image_file>\n";
    std::cerr << "Converts image to PNG format.\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  -l, --list    List available codecs\n";
    std::cerr << "  -h, --help    Show this help\n";
}

void list_codecs() {
    std::cout << "Available codecs:\n";
    const auto& registry = onyx_image::codec_registry::instance();
    for (std::size_t i = 0; i < registry.decoder_count(); ++i) {
        const auto* decoder = registry.decoder_at(i);
        std::cout << "  " << decoder->name() << " (";
        bool first = true;
        for (const auto& ext : decoder->extensions()) {
            if (!first) std::cout << ", ";
            std::cout << ext;
            first = false;
        }
        std::cout << ")\n";
    }
}

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

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    // Check for options
    if (std::strcmp(argv[1], "-l") == 0 || std::strcmp(argv[1], "--list") == 0) {
        list_codecs();
        return 0;
    }

    if (std::strcmp(argv[1], "-h") == 0 || std::strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    const std::filesystem::path input_path(argv[1]);

    if (!std::filesystem::exists(input_path)) {
        std::cerr << "Error: File not found: " << input_path << "\n";
        return 1;
    }

    // Read input file
    auto data = read_file(input_path);
    if (data.empty()) {
        std::cerr << "Error: Failed to read file: " << input_path << "\n";
        return 1;
    }

    // Find decoder by sniffing
    const auto* decoder = onyx_image::codec_registry::instance().find_decoder(data);
    if (!decoder) {
        std::cerr << "Error: Unknown image format: " << input_path << "\n";
        return 1;
    }

    std::cout << "Detected format: " << decoder->name() << "\n";

    // Decode image
    onyx_image::png_surface surface;
    auto result = decoder->decode(data, surface, {});
    if (!result) {
        std::cerr << "Error: Failed to decode: " << result.message << "\n";
        return 1;
    }

    std::cout << "Decoded: " << surface.width() << "x" << surface.height() << "\n";

    // Use second argument as output path, or same name with .png extension
    std::filesystem::path output_path;
    if (argc >= 3) {
        output_path = argv[2];
    } else {
        output_path = input_path;
        output_path.replace_extension(".png");
    }

    if (!surface.save(output_path)) {
        std::cerr << "Error: Failed to save: " << output_path << "\n";
        return 1;
    }

    std::cout << "Saved: " << output_path << "\n";

    return 0;
}
