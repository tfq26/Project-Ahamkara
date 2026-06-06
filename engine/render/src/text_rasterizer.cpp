#include "ae/render/text_rasterizer.h"

#include <memory>

namespace ae::render {

#if defined(__APPLE__)
std::unique_ptr<TextRasterizer> create_mac_text_rasterizer();
#endif

std::unique_ptr<TextRasterizer> create_platform_text_rasterizer() {
#if defined(__APPLE__)
    return create_mac_text_rasterizer();
#else
    return {};
#endif
}

}  // namespace ae::render
