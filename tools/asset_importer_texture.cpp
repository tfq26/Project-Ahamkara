#include "asset_importer_texture.h"
#include "ae/render/compiled_texture.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

namespace asset_importer {

bool load_tga_texture(const std::filesystem::path& path, ae::render::TextureAsset& texture, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "Failed to open texture source";
        return false;
    }

    std::uint8_t header[18] = {};
    file.read(reinterpret_cast<char*>(header), sizeof(header));
    if (!file) {
        error = "Failed to read TGA header";
        return false;
    }

    const std::uint8_t id_length = header[0];
    const std::uint8_t color_map_type = header[1];
    const std::uint8_t image_type = header[2];
    const std::uint16_t width = static_cast<std::uint16_t>(header[12]) |
                                (static_cast<std::uint16_t>(header[13]) << 8U);
    const std::uint16_t height = static_cast<std::uint16_t>(header[14]) |
                                 (static_cast<std::uint16_t>(header[15]) << 8U);
    const std::uint8_t bits_per_pixel = header[16];
    const std::uint8_t image_descriptor = header[17];

    if (color_map_type != 0) {
        error = "Color-mapped TGA textures are not supported";
        return false;
    }

    if (image_type != 2) {
        error = "Only uncompressed true-color TGA textures are supported";
        return false;
    }

    if (width == 0 || height == 0) {
        error = "TGA texture dimensions must be non-zero";
        return false;
    }

    if (bits_per_pixel != 24 && bits_per_pixel != 32) {
        error = "Only 24-bit and 32-bit TGA textures are supported";
        return false;
    }

    if (id_length > 0) {
        file.seekg(id_length, std::ios::cur);
        if (!file) {
            error = "Failed to skip TGA image ID field";
            return false;
        }
    }

    const std::size_t bytes_per_pixel = bits_per_pixel / 8U;
    const std::size_t source_bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * bytes_per_pixel;
    std::vector<std::uint8_t> source_pixels(source_bytes);
    file.read(reinterpret_cast<char*>(source_pixels.data()), static_cast<std::streamsize>(source_pixels.size()));
    if (!file) {
        error = "Failed to read TGA pixel payload";
        return false;
    }

    texture = {};
    texture.width = width;
    texture.height = height;
    texture.format = ae::render::CompiledTextureFormat::Rgba8;
    texture.pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);

    const bool top_left_origin = (image_descriptor & 0x20U) != 0;
    for (std::uint32_t y = 0; y < texture.height; ++y) {
        const std::uint32_t source_y = top_left_origin ? y : (texture.height - 1U - y);
        for (std::uint32_t x = 0; x < texture.width; ++x) {
            const std::size_t source_index =
                (static_cast<std::size_t>(source_y) * texture.width + x) * bytes_per_pixel;
            const std::size_t dest_index =
                (static_cast<std::size_t>(y) * texture.width + x) * 4U;

            texture.pixels[dest_index + 0] = source_pixels[source_index + 2];
            texture.pixels[dest_index + 1] = source_pixels[source_index + 1];
            texture.pixels[dest_index + 2] = source_pixels[source_index + 0];
            texture.pixels[dest_index + 3] = bytes_per_pixel == 4U ? source_pixels[source_index + 3] : 255U;
        }
    }

    return true;
}

bool compile_texture(const ImportEntry& entry) {
    const auto extension = entry.source.extension().generic_string();
    if (extension != ".tga" && extension != ".TGA") {
        std::cerr << "Texture import currently supports TGA sources only: " << entry.source << '\n';
        return false;
    }

    ae::render::TextureAsset texture;
    std::string error;
    if (!load_tga_texture(entry.source, texture, error)) {
        std::cerr << "Texture import failed for " << entry.source << ": " << error << '\n';
        return false;
    }

    if (!ae::render::save_compiled_texture(entry.output.string(), texture, error)) {
        std::cerr << "Failed to compile " << entry.source << " -> " << entry.output << ": " << error << '\n';
        return false;
    }

    std::cout << "texture " << entry.source << " -> " << entry.output << '\n';
    return true;
}

} // namespace asset_importer
