#include "ae/render/font_atlas.h"

#include "ae/core/log.h"
#include "ae/render/text_rasterizer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string_view>
#include <vector>

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace ae::render {
namespace {

constexpr int kFirstAscii = 32;
constexpr int kLastAscii = 126;
constexpr int kGlyphCount = kLastAscii - kFirstAscii + 1;
constexpr int kAtlasWidth = 512;
constexpr int kAtlasPadding = 2;
constexpr float kDefaultFontPixelSize = 48.0F;
constexpr float kBaseFontTargetSize = 10.0F;
constexpr std::string_view kDefaultFontName = "Menlo";

struct GlyphPlacement {
    int x {0};
    int y {0};
};

}  // namespace

struct FontAtlas::Impl {
    struct Glyph {
        float u0 {0.0F};
        float u1 {0.0F};
        float v0 {0.0F};
        float v1 {0.0F};
        float advance {0.0F};
        float bearing_x {0.0F};
        float bearing_y {0.0F};
        float width {0.0F};
        float height {0.0F};
        bool has_bitmap {false};
    };

    bool ready {false};
    GLuint texture_id {0};
    float ascent {0.0F};
    float base_line_height {0.0F};
    Glyph glyphs[kGlyphCount] {};
};

FontAtlas::~FontAtlas() {
    shutdown();
}

bool FontAtlas::initialize_default() {
    if (impl_ != nullptr && impl_->ready) {
        return true;
    }

    shutdown();
    impl_ = new Impl {};

    std::unique_ptr<TextRasterizer> rasterizer = create_platform_text_rasterizer();
    if (rasterizer == nullptr) {
        ae::log_warning("FontAtlas: no platform text rasterizer available, falling back to bitmap font.");
        shutdown();
        return false;
    }

    RasterizedFontFace face;
    if (!rasterizer->rasterize_ascii(kDefaultFontName, kDefaultFontPixelSize, face)) {
        ae::log_warning("FontAtlas: failed to rasterize '" + std::string(kDefaultFontName) + "' at " + std::to_string(kDefaultFontPixelSize) + " px, falling back to bitmap font.");
        shutdown();
        return false;
    }

    GlyphPlacement placements[kGlyphCount] {};
    int cursor_x = kAtlasPadding;
    int cursor_y = kAtlasPadding;
    int row_height = 0;
    int atlas_height = kAtlasPadding;

    for (const RasterizedGlyphBitmap& rasterized_glyph : face.glyphs) {
        if (rasterized_glyph.character < kFirstAscii || rasterized_glyph.character > kLastAscii) {
            continue;
        }

        const int glyph_index = rasterized_glyph.character - kFirstAscii;
        const int glyph_width = std::max(rasterized_glyph.width, 1);
        const int glyph_height = std::max(rasterized_glyph.height, 1);

        if (cursor_x + glyph_width + kAtlasPadding > kAtlasWidth) {
            cursor_x = kAtlasPadding;
            cursor_y += row_height + kAtlasPadding;
            row_height = 0;
        }

        placements[static_cast<std::size_t>(glyph_index)] = {cursor_x, cursor_y};
        cursor_x += glyph_width + kAtlasPadding;
        row_height = std::max(row_height, glyph_height);
        atlas_height = std::max(atlas_height, cursor_y + glyph_height + kAtlasPadding);
    }

    std::vector<unsigned char> pixels(
        static_cast<std::size_t>(kAtlasWidth * atlas_height * 4),
        0U);
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i + 0] = 255U;
        pixels[i + 1] = 255U;
        pixels[i + 2] = 255U;
        pixels[i + 3] = 0U;
    }

    impl_->ascent = face.ascent;
    impl_->base_line_height = face.line_height > 0.0F
        ? face.line_height
        : face.ascent + face.descent;

    for (const RasterizedGlyphBitmap& rasterized_glyph : face.glyphs) {
        if (rasterized_glyph.character < kFirstAscii || rasterized_glyph.character > kLastAscii) {
            continue;
        }

        const std::size_t glyph_index = static_cast<std::size_t>(rasterized_glyph.character - kFirstAscii);
        const GlyphPlacement placement = placements[glyph_index];
        auto& glyph = impl_->glyphs[glyph_index];
        glyph.advance = std::max(rasterized_glyph.advance, 0.0F);
        glyph.bearing_x = rasterized_glyph.bearing_x;
        glyph.bearing_y = rasterized_glyph.bearing_y;
        glyph.width = static_cast<float>(rasterized_glyph.width);
        glyph.height = static_cast<float>(rasterized_glyph.height);

        if (rasterized_glyph.width <= 0 || rasterized_glyph.height <= 0) {
            continue;
        }

        for (int row = 0; row < rasterized_glyph.height; ++row) {
            const unsigned char* source =
                rasterized_glyph.alpha_pixels.data() + static_cast<std::size_t>(row * rasterized_glyph.width);
            for (int col = 0; col < rasterized_glyph.width; ++col) {
                const std::size_t pixel_index = static_cast<std::size_t>(
                    (placement.y + row) * kAtlasWidth + (placement.x + col));
                pixels[pixel_index * 4 + 3] = source[col];
            }
        }

        glyph.u0 = static_cast<float>(placement.x) / static_cast<float>(kAtlasWidth);
        glyph.u1 = static_cast<float>(placement.x + rasterized_glyph.width) / static_cast<float>(kAtlasWidth);
        glyph.v0 = static_cast<float>(placement.y) / static_cast<float>(atlas_height);
        glyph.v1 = static_cast<float>(placement.y + rasterized_glyph.height) / static_cast<float>(atlas_height);
        glyph.has_bitmap = true;
    }

    glGenTextures(1, &impl_->texture_id);
    glBindTexture(GL_TEXTURE_2D, impl_->texture_id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        kAtlasWidth,
        atlas_height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    impl_->ready = true;
    return true;
}

void FontAtlas::shutdown() {
    if (impl_ == nullptr) {
        return;
    }

    if (impl_->texture_id != 0) {
        glDeleteTextures(1, &impl_->texture_id);
    }

    delete impl_;
    impl_ = nullptr;
}

bool FontAtlas::is_ready() const {
    return impl_ != nullptr && impl_->ready;
}

float FontAtlas::measure_text(std::string_view text, float scale) const {
    if (!is_ready()) {
        return static_cast<float>(text.size()) * 6.0F * scale;
    }

    const float size_multiplier = (kBaseFontTargetSize / kDefaultFontPixelSize) * scale;
    float width = 0.0F;
    for (const char character : text) {
        if (character < kFirstAscii || character > kLastAscii) {
            width += 10.0F * size_multiplier;
            continue;
        }

        const auto& glyph = impl_->glyphs[static_cast<std::size_t>(character - kFirstAscii)];
        width += glyph.advance * size_multiplier;
    }

    return width;
}

float FontAtlas::line_height(float scale) const {
    if (!is_ready()) {
        return 8.0F * scale;
    }

    const float size_multiplier = (kBaseFontTargetSize / kDefaultFontPixelSize) * scale;
    return impl_->base_line_height * size_multiplier;
}

void FontAtlas::draw_text(float x, float y, float scale, std::string_view text) const {
    if (!is_ready()) {
        return;
    }

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, impl_->texture_id);
    glBegin(GL_QUADS);

    const float size_multiplier = (kBaseFontTargetSize / kDefaultFontPixelSize) * scale;
    float cursor_x = x;
    const float baseline_y = std::round(y + impl_->ascent * size_multiplier);
    for (const char character : text) {
        if (character < kFirstAscii || character > kLastAscii) {
            cursor_x += 10.0F * size_multiplier;
            continue;
        }

        const auto& glyph = impl_->glyphs[static_cast<std::size_t>(character - kFirstAscii)];
        if (glyph.has_bitmap) {
            const float left = std::round(cursor_x + glyph.bearing_x * size_multiplier);
            const float top = std::round(baseline_y - glyph.bearing_y * size_multiplier);
            const float right = std::round(left + glyph.width * size_multiplier);
            const float bottom = std::round(top + glyph.height * size_multiplier);

            glTexCoord2f(glyph.u0, glyph.v0); glVertex2f(left, top);
            glTexCoord2f(glyph.u1, glyph.v0); glVertex2f(right, top);
            glTexCoord2f(glyph.u1, glyph.v1); glVertex2f(right, bottom);
            glTexCoord2f(glyph.u0, glyph.v1); glVertex2f(left, bottom);
        }

        cursor_x += glyph.advance * size_multiplier;
    }

    glEnd();
    glDisable(GL_TEXTURE_2D);
}

}  // namespace ae::render
