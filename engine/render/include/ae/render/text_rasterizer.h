#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace ae::render {

struct RasterizedGlyphBitmap {
    char character {' '};
    int width {0};
    int height {0};
    float bearing_x {0.0F};
    float bearing_y {0.0F};
    float advance {0.0F};
    std::vector<std::uint8_t> alpha_pixels {};
};

struct RasterizedFontFace {
    float ascent {0.0F};
    float descent {0.0F};
    float line_height {0.0F};
    std::vector<RasterizedGlyphBitmap> glyphs {};
};

class TextRasterizer {
public:
    virtual ~TextRasterizer() = default;

    virtual bool rasterize_ascii(std::string_view font_name,
                                 float pixel_size,
                                 RasterizedFontFace& out_face) = 0;
};

[[nodiscard]] std::unique_ptr<TextRasterizer> create_platform_text_rasterizer();

}  // namespace ae::render
