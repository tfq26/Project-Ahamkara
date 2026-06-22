#include "ae/render/compiled_texture.h"
#include "ae/render/binary_io.h"

#include <filesystem>
#include <fstream>

namespace ae::render {

bool save_compiled_texture(const std::string& path, const TextureAsset& texture, std::string& error) {
    error.clear();

    if (texture.width == 0 || texture.height == 0) {
        error = "Texture dimensions must be non-zero";
        return false;
    }

    if (texture.format != CompiledTextureFormat::Rgba8) {
        error = "Unsupported compiled texture format";
        return false;
    }

    const std::size_t expected_size =
        static_cast<std::size_t>(texture.width) * static_cast<std::size_t>(texture.height) * 4U;
    if (texture.pixels.size() != expected_size) {
        error = "Texture pixel payload size does not match RGBA8 dimensions";
        return false;
    }

    const std::filesystem::path output_path(path);
    if (!output_path.parent_path().empty()) {
        std::error_code create_error;
        std::filesystem::create_directories(output_path.parent_path(), create_error);
        if (create_error) {
            error = "Failed to create output directory: " + create_error.message();
            return false;
        }
    }

    std::ofstream file(output_path, std::ios::binary);
    if (!file) {
        error = "Failed to create: " + path;
        return false;
    }

    const std::uint32_t pixel_bytes = static_cast<std::uint32_t>(texture.pixels.size());
    if (!write_value(file, CompiledTextureFile::magic) ||
        !write_value(file, CompiledTextureFile::version) ||
        !write_value(file, texture.width) ||
        !write_value(file, texture.height) ||
        !write_value(file, texture.format) ||
        !write_value(file, pixel_bytes) ||
        !write_bytes(file, texture.pixels.data(), texture.pixels.size())) {
        error = "Failed to write compiled texture";
        return false;
    }

    return true;
}

bool CompiledTextureLoader::load(const std::string& path, TextureAsset& texture) {
    error_.clear();
    texture = {};

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error_ = "Failed to open: " + path;
        return false;
    }

    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    if (!read_value(file, magic) || !read_value(file, version)) {
        error_ = "Failed to read compiled texture header";
        return false;
    }

    if (magic != CompiledTextureFile::magic) {
        error_ = "Invalid compiled texture magic";
        return false;
    }

    if (version != CompiledTextureFile::version) {
        error_ = "Unsupported compiled texture version";
        return false;
    }

    std::uint32_t format = 0;
    std::uint32_t pixel_bytes = 0;
    if (!read_value(file, texture.width) ||
        !read_value(file, texture.height) ||
        !read_value(file, format) ||
        !read_value(file, pixel_bytes)) {
        error_ = "Failed to read compiled texture metadata";
        return false;
    }

    texture.format = static_cast<CompiledTextureFormat>(format);
    if (texture.format != CompiledTextureFormat::Rgba8) {
        error_ = "Unsupported compiled texture format";
        return false;
    }

    const std::size_t expected_size =
        static_cast<std::size_t>(texture.width) * static_cast<std::size_t>(texture.height) * 4U;
    if (pixel_bytes != expected_size) {
        error_ = "Compiled texture payload size does not match metadata";
        return false;
    }

    texture.pixels.resize(pixel_bytes);
    if (!texture.pixels.empty() && !read_bytes(file, texture.pixels.data(), texture.pixels.size())) {
        error_ = "Failed to read compiled texture pixels";
        return false;
    }

    return true;
}

} // namespace ae::render
