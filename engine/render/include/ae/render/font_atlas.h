#pragma once

#include <string_view>

namespace ae::render {

class FontAtlas {
public:
    FontAtlas() = default;
    FontAtlas(const FontAtlas&) = delete;
    FontAtlas& operator=(const FontAtlas&) = delete;
    ~FontAtlas();

    bool initialize_default();
    void shutdown();

    [[nodiscard]] bool is_ready() const;
    [[nodiscard]] float measure_text(std::string_view text, float scale) const;
    [[nodiscard]] float line_height(float scale) const;

    void draw_text(float x, float y, float scale, std::string_view text,
                   float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f) const;

private:
    struct Impl;
    Impl* impl_ {nullptr};
};

}  // namespace ae::render

