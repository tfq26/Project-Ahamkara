#include "ae/render/text_rasterizer.h"

#if defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace ae::render {
namespace {

constexpr int kFirstAscii = 32;
constexpr int kLastAscii = 126;

class MacTextRasterizer final : public TextRasterizer {
public:
    bool rasterize_ascii(std::string_view font_name,
                         float pixel_size,
                         RasterizedFontFace& out_face) override {
        out_face = {};

        CFStringRef font_name_ref = CFStringCreateWithBytes(
            nullptr,
            reinterpret_cast<const UInt8*>(font_name.data()),
            static_cast<CFIndex>(font_name.size()),
            kCFStringEncodingUTF8,
            false);
        if (font_name_ref == nullptr) {
            return false;
        }

        CTFontRef font = CTFontCreateWithName(font_name_ref, pixel_size, nullptr);
        CFRelease(font_name_ref);
        if (font == nullptr) {
            return false;
        }

        out_face.ascent = static_cast<float>(CTFontGetAscent(font));
        out_face.descent = static_cast<float>(CTFontGetDescent(font));
        out_face.line_height = out_face.ascent + out_face.descent + static_cast<float>(CTFontGetLeading(font));
        if (out_face.line_height <= 0.0F) {
            out_face.line_height = out_face.ascent + out_face.descent;
        }

        out_face.glyphs.reserve(static_cast<std::size_t>(kLastAscii - kFirstAscii + 1));

        for (int ascii = kFirstAscii; ascii <= kLastAscii; ++ascii) {
            UniChar character = static_cast<UniChar>(ascii);
            CGGlyph glyph = 0;
            if (!CTFontGetGlyphsForCharacters(font, &character, &glyph, 1)) {
                continue;
            }

            CGSize advance_size {};
            CTFontGetAdvancesForGlyphs(font, kCTFontOrientationHorizontal, &glyph, &advance_size, 1);

            CGRect bounds = CTFontGetBoundingRectsForGlyphs(font, kCTFontOrientationHorizontal, &glyph, nullptr, 1);
            const float raw_min_x = static_cast<float>(CGRectGetMinX(bounds));
            const float raw_min_y = static_cast<float>(CGRectGetMinY(bounds));
            const float raw_max_x = static_cast<float>(CGRectGetMaxX(bounds));
            const float raw_max_y = static_cast<float>(CGRectGetMaxY(bounds));

            // Floor / ceil the bounding box to get a full-pixel bitmap that comfortably
            // contains the glyph, but keep the raw floating-point bearings for subpixel-
            // accurate placement inside the atlas.
            const int snapped_min_x = static_cast<int>(std::floor(raw_min_x));
            const int snapped_min_y = static_cast<int>(std::floor(raw_min_y));
            const int snapped_max_x = static_cast<int>(std::ceil(raw_max_x));
            const int snapped_max_y = static_cast<int>(std::ceil(raw_max_y));

            RasterizedGlyphBitmap rasterized_glyph;
            rasterized_glyph.character = static_cast<char>(ascii);
            rasterized_glyph.width = std::max(0, snapped_max_x - snapped_min_x);
            rasterized_glyph.height = std::max(0, snapped_max_y - snapped_min_y);
            rasterized_glyph.bearing_x = raw_min_x;
            rasterized_glyph.bearing_y = raw_max_y;
            rasterized_glyph.advance = std::max(static_cast<float>(advance_size.width), 0.0F);

            if (rasterized_glyph.width > 0 && rasterized_glyph.height > 0) {
                rasterized_glyph.alpha_pixels.resize(
                    static_cast<std::size_t>(rasterized_glyph.width * rasterized_glyph.height),
                    0U);

                CGColorSpaceRef color_space = CGColorSpaceCreateDeviceGray();
                CGContextRef context = CGBitmapContextCreate(
                    rasterized_glyph.alpha_pixels.data(),
                    static_cast<std::size_t>(rasterized_glyph.width),
                    static_cast<std::size_t>(rasterized_glyph.height),
                    8,
                    static_cast<std::size_t>(rasterized_glyph.width),
                    color_space,
                    kCGImageAlphaNone);
                CGColorSpaceRelease(color_space);

                if (context == nullptr) {
                    CFRelease(font);
                    return false;
                }

                CGContextSetShouldAntialias(context, true);
                CGContextSetAllowsAntialiasing(context, true);
                // Disable LCD subpixel font smoothing — we only have a single-channel
                // grayscale bitmap, so RGB subpixel fringing would collapse to noise.
                CGContextSetShouldSmoothFonts(context, false);
                CGContextSetAllowsFontSmoothing(context, false);
                CGContextSetGrayFillColor(context, 1.0, 1.0);
                CGContextSetTextDrawingMode(context, kCGTextFill);

                CGPoint position {
                    static_cast<CGFloat>(-snapped_min_x),
                    static_cast<CGFloat>(-snapped_min_y)
                };
                CTFontDrawGlyphs(font, &glyph, &position, 1, context);
                CGContextRelease(context);
            }

            if (rasterized_glyph.advance <= 0.0F) {
                rasterized_glyph.advance = static_cast<float>(rasterized_glyph.width);
            }

            out_face.glyphs.push_back(std::move(rasterized_glyph));
        }

        CFRelease(font);
        return !out_face.glyphs.empty();
    }
};

}  // namespace

std::unique_ptr<TextRasterizer> create_mac_text_rasterizer() {
    return std::make_unique<MacTextRasterizer>();
}

}  // namespace ae::render

#endif
