#pragma once

#include "imgui.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ae::ui {

/// Gameplay state snapshot passed to HUD elements each frame.
struct HudState {
    // Player
    float health{100.0f};
    float max_health{100.0f};
    float shield{0.0f};
    float max_shield{0.0f};
    int ammo_current{30};
    int ammo_max{30};
    int reserve_ammo{150};
    int weapon_index{0};
    const char* weapon_name{"AR-15"};

    // Abilities
    bool grenade_available{true};
    float grenade_cooldown{0.0f};   // 0.0 = ready, 1.0 = full cooldown
    int grenade_count{1};

    bool special_available{true};
    float special_cooldown{0.0f};

    bool artifact_available{true};
    float artifact_cooldown{0.0f};

    float ultimate_charge{0.0f};     // 0.0-1.0
    bool ultimate_ready{false};

    // Match
    float match_time{0.0f};

    // Display
    bool crosshair_visible{true};
    float crosshair_spread{0.0f};    // dynamic spread from movement/fire
};

/// Base class for a HUD overlay element.  Subclasses implement render()
/// to draw this element at its configured position on screen.
class HudElement {
public:
    virtual ~HudElement() = default;

    /// Configure position from JSON-like data.
    void set_layout(float x, float y, const std::string& anchor,
                    float width, float height);

    /// Render this element at its configured position.
    virtual void render(float screen_w, float screen_h, const HudState& state) = 0;

    [[nodiscard]] bool visible() const { return visible_; }
    void set_visible(bool v) { visible_ = v; }

protected:
    void compute_position(float screen_w, float screen_h, float el_w, float el_h,
                          float& out_x, float& out_y) const;

    float layout_x_{0}, layout_y_{0};
    float layout_w_{0}, layout_h_{0};
    std::string anchor_{"top_left"};
    bool visible_{true};
};

// ── Concrete HUD elements ────────────────────────────────────────────────────

class HudHealthBar : public HudElement {
public:
    void render(float sw, float sh, const HudState& state) override;
};

class HudAmmoCounter : public HudElement {
public:
    void render(float sw, float sh, const HudState& state) override;
};

class HudAbilityIcon : public HudElement {
public:
    enum AbilityType { Grenade, Special, Artifact, Ultimate };
    explicit HudAbilityIcon(AbilityType type) : type_(type) {}
    void render(float sw, float sh, const HudState& state) override;

private:
    AbilityType type_;
    static const char* label_for(AbilityType t);
    static ImVec4 color_for(AbilityType t);
    float cooldown_for(const HudState& s, AbilityType t) const;
    bool available_for(const HudState& s, AbilityType t) const;
    int count_for(const HudState& s, AbilityType t) const;
};

class HudCrosshair : public HudElement {
public:
    void render(float sw, float sh, const HudState& state) override;
};

// ── Factory ───────────────────────────────────────────────────────────────────

using HudElementFactory = std::function<std::unique_ptr<HudElement>()>;

/// Register element types so the HUD system can create them from JSON.
class HudElementRegistry {
public:
    static HudElementRegistry& instance();

    void register_type(const std::string& name, HudElementFactory factory);
    [[nodiscard]] std::unique_ptr<HudElement> create(const std::string& type) const;

private:
    std::unordered_map<std::string, HudElementFactory> factories_;
};

/// Convenience: register a concrete type. Call at static init.
template <typename T>
struct HudElementRegistrar {
    HudElementRegistrar(const std::string& name) {
        HudElementRegistry::instance().register_type(name, [] { return std::make_unique<T>(); });
    }
};

}  // namespace ae::ui
