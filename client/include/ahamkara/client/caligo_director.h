#pragma once

#include "ae/render/compiled_level.h"
#include "ae/render/level_render.h"

namespace ae::render {
class RenderBackend;
struct DebugScene;
} // namespace ae::render

namespace ahamkara::client {

/// Presentation-only Director scene. It is separate from gameplay simulation
/// and supplies the static 3D view shown while Deploy is open.
class CaligoDirector {
  public:
    enum class View {
        Exterior,
        Cutaway,
    };

    CaligoDirector() = default;
    CaligoDirector(const CaligoDirector&) = delete;
    CaligoDirector& operator=(const CaligoDirector&) = delete;

    bool initialize(ae::render::RenderBackend& backend);
    void destroy(ae::render::RenderBackend& backend);

    void set_view(View view) {
        view_ = view;
    }
    [[nodiscard]] View view() const {
        return view_;
    }
    [[nodiscard]] bool ready() const {
        return ready_;
    }
    [[nodiscard]] const ae::render::LevelRenderScene* render_scene() const;
    void apply_camera(ae::render::DebugScene& scene) const;

  private:
    ae::render::LevelRenderScene exterior_scene_ {};
    ae::render::LevelRenderScene cutaway_scene_ {};
    ae::render::LevelAsset exterior_asset_ {};
    ae::render::LevelAsset cutaway_asset_ {};
    View view_ {View::Exterior};
    bool ready_ {false};
};

} // namespace ahamkara::client
