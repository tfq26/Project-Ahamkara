#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ae::render {

enum class CompiledTextureFormat : std::uint32_t {
    Rgba8 = 1,
};

struct TextureAsset {
    std::uint32_t width {0};
    std::uint32_t height {0};
    CompiledTextureFormat format {CompiledTextureFormat::Rgba8};
    std::vector<std::uint8_t> pixels;
};

struct CompiledTextureFile {
    static constexpr std::uint32_t magic = 0x58455441; // ATEX
    static constexpr std::uint32_t version = 1;
};

[[nodiscard]] bool save_compiled_texture(const std::string& path, const TextureAsset& texture, std::string& error);

class CompiledTextureLoader {
public:
    [[nodiscard]] bool load(const std::string& path, TextureAsset& texture);

    [[nodiscard]] const std::string& last_error() const { return error_; }

private:
    std::string error_;
};

} // namespace ae::render
