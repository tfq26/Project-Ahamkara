#pragma once

#include "wish/types.h"
#include "wish/core/activity.h"
#include "wish/core/activity_modifier.h"
#include "wish/core/modifier_types.h"
#include "wish/log.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace wish::core {

// ── Non-template helper functions (defined in live_content_hooks.cpp) ────

/// Version identifier for the live content hooks data contract.
std::string live_content_hooks_version();

/// Parse a float parameter value from the first matching key in a modifier.
float get_modifier_float_param(const ModifierConfig& config,
                                std::string_view key,
                                float default_val = 0.0F);

/// Parse an int parameter value from the first matching key in a modifier.
int get_modifier_int_param(const ModifierConfig& config,
                            std::string_view key,
                            int default_val = 0);

/// Parse a bool parameter value from the first matching key in a modifier.
bool get_modifier_bool_param(const ModifierConfig& config,
                              std::string_view key,
                              bool default_val = false);

/// LiveContentModifier wraps a base activity and injects data-driven
/// modifier hooks that can alter gameplay parameters at runtime without
/// code changes.
///
/// Modifiers are configured via ActivityConfig::modifiers (loaded from JSON
/// by ActivityLoader). The modifier can be configured for automatic rotation
/// over time so that different modifiers become active on a schedule.
///
/// Usage:
///   using LiveDeathmatch = LiveContentModifier<DeathmatchActivity>;
///
///   auto dm = std::make_unique<LiveDeathmatch>(std::move(base_dm));
///   if (dm->initialize(config)) {
///       manager.start_activity(config.id, std::move(dm));
///   }
///
/// Game code can then query:
///   if (dm->has_active_modifier(ModifierType::DamageMultiplier)) {
///       auto mult = get_modifier_param_value(dm, "multiplier");
///       damage *= std::stof(std::string(mult));
///   }
template <typename BaseActivity>
class LiveContentModifier : public ActivityModifier<BaseActivity> {
public:
    using ActivityModifier<BaseActivity>::ActivityModifier;

    /// Initialize with config. Copies modifier configurations and sets up
    /// the rotation state.
    bool initialize(const ActivityConfig& cfg) override {
        // Store a copy of the modifier configs for the query API.
        modifiers_.clear();
        for (const auto& m : cfg.modifiers) {
            ModifierState state {};
            state.config = m;
            state.config.remaining_time = m.duration; // reset runtime timer
            modifiers_.push_back(std::move(state));
        }

        rotation_enabled_ = cfg.modifier_rotation_enabled;
        rotation_timer_ = 0.0F;
        rotation_interval_ = 30.0F; // default: rotate every 30 seconds
        current_rotation_index_ = 0;

        // If rotation is enabled, only the first modifier (or indexed by rotation_order)
        // starts active.
        if (rotation_enabled_ && !modifiers_.empty()) {
            activate_rotation_slot(0);
        }

        return this->base().initialize(cfg);
    }

    /// Per-tick update: manages modifier timer decay and rotation.
    void tick(float dt) override {
        // Tick modifier timers
        for (auto& ms : modifiers_) {
            if (ms.config.active && ms.config.duration > 0.0F) {
                ms.config.remaining_time -= dt;
                if (ms.config.remaining_time <= 0.0F) {
                    ms.config.remaining_time = 0.0F;
                    ms.config.active = false;
                }
            }
        }

        // Handle rotation
        if (rotation_enabled_ && modifiers_.size() > 1) {
            rotation_timer_ += dt;
            if (rotation_timer_ >= rotation_interval_) {
                rotate_next();
                rotation_timer_ = 0.0F;
            }
        }

        this->base().tick(dt);
    }

    // ── Query API ────────────────────────────────────────────────────────

    /// Check whether any active modifier of the given type exists.
    [[nodiscard]] bool has_active_modifier(ModifierType type) const {
        return get_modifier(type) != nullptr;
    }

    /// Check whether any active modifier with the given name exists.
    [[nodiscard]] bool has_active_modifier(std::string_view name) const {
        return get_modifier(name) != nullptr;
    }

    /// Get the first active ModifierConfig of the given type, or nullptr.
    [[nodiscard]] const ModifierConfig* get_modifier(ModifierType type) const {
        for (const auto& ms : modifiers_) {
            if (ms.config.type == type && is_modifier_active(ms.config)) {
                return &ms.config;
            }
        }
        return nullptr;
    }

    /// Get the first active ModifierConfig with the given name, or nullptr.
    [[nodiscard]] const ModifierConfig* get_modifier(std::string_view name) const {
        for (const auto& ms : modifiers_) {
            if (ms.config.name == name && is_modifier_active(ms.config)) {
                return &ms.config;
            }
        }
        return nullptr;
    }

    /// Get a parameter value from the first active modifier of the given type.
    [[nodiscard]] std::string_view get_param(ModifierType type, std::string_view key) const {
        const auto* mod = get_modifier(type);
        if (!mod) return {};
        return find_modifier_param(*mod, key);
    }

    /// Get a parameter value from the first active modifier with the given name.
    [[nodiscard]] std::string_view get_param(std::string_view name, std::string_view key) const {
        const auto* mod = get_modifier(name);
        if (!mod) return {};
        return find_modifier_param(*mod, key);
    }

    /// Return all modifier configs (active and inactive) for inspection.
    [[nodiscard]] std::vector<ModifierConfig> all_modifiers() const {
        std::vector<ModifierConfig> result;
        result.reserve(modifiers_.size());
        for (const auto& ms : modifiers_) {
            result.push_back(ms.config);
        }
        return result;
    }

    /// Return only active modifier configs.
    [[nodiscard]] std::vector<ModifierConfig> active_modifiers() const {
        std::vector<ModifierConfig> result;
        for (const auto& ms : modifiers_) {
            if (is_modifier_active(ms.config)) {
                result.push_back(ms.config);
            }
        }
        return result;
    }

    // ── Rotation controls ─────────────────────────────────────────────────

    void enable_rotation(bool enabled) { rotation_enabled_ = enabled; }
    bool rotation_enabled() const { return rotation_enabled_; }

    void set_rotation_interval(float interval_seconds) {
        rotation_interval_ = std::max(0.1F, interval_seconds);
    }

    float rotation_interval() const { return rotation_interval_; }

    /// Manually advance to the next rotation slot.
    void rotate_next() {
        if (modifiers_.empty()) return;
        // Deactivate all
        for (auto& ms : modifiers_) {
            ms.config.active = false;
        }
        // Activate next slot
        current_rotation_index_ = (current_rotation_index_ + 1) % modifiers_.size();
        activate_rotation_slot(current_rotation_index_);
    }

    /// Reset rotation to the first slot.
    void rotate_reset() {
        current_rotation_index_ = 0;
        if (!modifiers_.empty()) {
            activate_rotation_slot(0);
        }
    }

private:
    struct ModifierState {
        ModifierConfig config {};
    };

    void activate_rotation_slot(wish::u32 index) {
        if (index >= modifiers_.size()) return;
        for (auto& ms : modifiers_) {
            ms.config.active = false;
        }
        modifiers_[index].config.active = true;
        modifiers_[index].config.remaining_time = modifiers_[index].config.duration;
    }

    std::vector<ModifierState> modifiers_;
    bool rotation_enabled_ {false};
    float rotation_interval_ {30.0F};
    float rotation_timer_ {0.0F};
    wish::u32 current_rotation_index_ {0};
};

} // namespace wish::core
