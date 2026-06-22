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
#include <OpenGL/gl3.h>
#else
#include <GL/glcorearb.h>
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

// Font shader — GLSL 330 Core Profile
static const char* kFontVS =
    "#version 330 core\n"
    "layout(location = 0) in vec2 aPos;\n"
    "layout(location = 1) in vec2 aTexCoord;\n"
    "layout(location = 2) in vec4 aColor;\n"
    "uniform mat4 uProjection;\n"
    "out vec2 vTexCoord;\n"
    "out vec4 vColor;\n"
    "void main() {\n"
    "    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);\n"
    "    vTexCoord = aTexCoord;\n"
    "    vColor = aColor;\n"
    "}\n";

static const char* kFontFS =
    "#version 330 core\n"
    "in vec2 vTexCoord;\n"
    "in vec4 vColor;\n"
    "uniform sampler2D uTexture;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    float a = texture(uTexture, vTexCoord).a;\n"
    "    fragColor = vec4(vColor.rgb, vColor.a * a);\n"
    "}\n";

static GLuint compile_font_shader(const char* vs, const char* fs) {
    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v, 1, &vs, nullptr);
    glCompileShader(v);
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f, 1, &fs, nullptr);
    glCompileShader(f);
    GLuint p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f);
    glBindAttribLocation(p, 0, "aPos");
    glBindAttribLocation(p, 1, "aTexCoord");
    glBindAttribLocation(p, 2, "aColor");
    glLinkProgram(p);
    glDeleteShader(v); glDeleteShader(f);
    return p;
}

// Batch vertex for text quads
struct TextVertex {
    float x, y;
    float u, v;
    float r, g, b, a;
};

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

    // Core Profile font rendering resources
    GLuint font_program {0};
    GLuint font_vao {0};
    GLuint font_vbo {0};
    GLint  u_font_proj_loc {-1};
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

    impl_->font_program = compile_font_shader(kFontVS, kFontFS);
    impl_->u_font_proj_loc = glGetUniformLocation(impl_->font_program, "uProjection");

    glGenVertexArrays(1, &impl_->font_vao);
    glGenBuffers(1, &impl_->font_vbo);

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
    if (impl_->font_program != 0) {
        glDeleteProgram(impl_->font_program);
    }
    if (impl_->font_vao != 0) {
        glDeleteVertexArrays(1, &impl_->font_vao);
    }
    if (impl_->font_vbo != 0) {
        glDeleteBuffers(1, &impl_->font_vbo);
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

void FontAtlas::draw_text(float x, float y, float scale, std::string_view text,
                          float r, float g, float b, float a) const {
    if (!is_ready()) return;

    // Build glyph quads on CPU
    std::vector<TextVertex> verts;
    const float size_multiplier = (kBaseFontTargetSize / kDefaultFontPixelSize) * scale;
    float cursor_x = x;
    const float baseline_y = std::round(y + impl_->ascent * size_multiplier);

    for (const char character : text) {
        if (character < kFirstAscii || character > kLastAscii) {
            cursor_x += 10.0F * size_multiplier;
            continue;
        }
        const auto& glyph = impl_->glyphs[static_cast<std::size_t>(character - kFirstAscii)];
        if (!glyph.has_bitmap) {
            cursor_x += glyph.advance * size_multiplier;
            continue;
        }

        const float left   = std::round(cursor_x + glyph.bearing_x * size_multiplier);
        const float top    = std::round(baseline_y - glyph.bearing_y * size_multiplier);
        const float right  = std::round(left + glyph.width * size_multiplier);
        const float bottom = std::round(top + glyph.height * size_multiplier);

        verts.push_back({left,  top,    glyph.u0, glyph.v0, r,g,b,a});
        verts.push_back({right, top,    glyph.u1, glyph.v0, r,g,b,a});
        verts.push_back({right, bottom, glyph.u1, glyph.v1, r,g,b,a});
        verts.push_back({left,  top,    glyph.u0, glyph.v0, r,g,b,a});
        verts.push_back({right, bottom, glyph.u1, glyph.v1, r,g,b,a});
        verts.push_back({left,  bottom, glyph.u0, glyph.v1, r,g,b,a});

        cursor_x += glyph.advance * size_multiplier;
    }

    if (verts.empty()) return;

    // Get viewport for orthographic projection
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    float ortho[16] = {
        2.0f/vp[2], 0, 0, 0,
        0, -2.0f/vp[3], 0, 0,
        0, 0, -1, 0,
        -1, 1, 0, 1
    };

    // Save state
    GLboolean depth_was_on = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blend_was_on = glIsEnabled(GL_BLEND);
    GLint prev_program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(impl_->font_program);
    glUniformMatrix4fv(impl_->u_font_proj_loc, 1, GL_FALSE, ortho);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, impl_->texture_id);

    glBindVertexArray(impl_->font_vao);
    glBindBuffer(GL_ARRAY_BUFFER, impl_->font_vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(TextVertex),
                 verts.data(), GL_STREAM_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex),
                          (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex),
                          (void*)(sizeof(float)*2));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(TextVertex),
                          (void*)(sizeof(float)*4));
    glEnableVertexAttribArray(2);

    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)verts.size());

    // Restore state
    glBindVertexArray(0);
    glUseProgram((GLuint)prev_program);
    if (depth_was_on) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (!blend_was_on) glDisable(GL_BLEND);
}

}  // namespace ae::render
