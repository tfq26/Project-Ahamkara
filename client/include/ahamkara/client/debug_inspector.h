#pragma once

#include <string>

namespace ahamkara::client {

struct ClientSimulationSnapshot;

/**
 * @brief Thin debug inspector overlay showing entity/component state.
 *
 * Renders an ImGui window with key simulation state pulled from the
 * current ClientSimulationSnapshot. Toggled via a hotkey (F2).
 *
 * This is a debugging tool, not gameplay code. It does not modify
 * any simulation state — only reads and displays it.
 */
class DebugInspector {
public:
    DebugInspector() = default;

    /// Toggle the inspector overlay on/off.
    void toggle() { visible_ = !visible_; }

    /// Set visibility explicitly.
    void set_visible(bool v) { visible_ = v; }

    [[nodiscard]] bool visible() const { return visible_; }

    /// Render the inspector ImGui window.
    /// Call during the UI stage, after begin_ui_frame().
    void render(const ClientSimulationSnapshot& snapshot);

private:
    void render_entity_inspector(const ClientSimulationSnapshot& snapshot);
    void render_performance_panel(const ClientSimulationSnapshot& snapshot);
    void render_weapon_inspector(const ClientSimulationSnapshot& snapshot);

    bool visible_ {false};
    int selected_tab_ {0};

    // Inspector state
    bool show_entity_tree_ {true};
    bool show_performance_ {false};
    bool show_weapon_details_ {false};
};

}  // namespace ahamkara::client
