#pragma once

/// PvEvP Modifier — wraps a PvP activity and adds PvE enemy waves.
/// Composes DeathmatchActivity + HordeActivity enemy spawning.
///
/// Usage:
///   using PvEvPActivity = PvEvPModifier<DeathmatchActivity>;
///
/// This is a concrete example of the ActivityModifier pattern (Phase 6).

#include "ahamkara/game/gameplay_types.h"
#include "ahamkara/game/net_types.h"
#include "wish/core/activity_modifier.h"

#include <cmath>

namespace ahamkara::game::activities {

/// Enemy state for the modifier (same layout as HordeActivity::EnemyState).
struct ModEnemyState {
    ae::u32 enemy_id {0};
    Vec3    position {};
    float   health {100.0F};
    bool    alive {true};
    ae::u32 enemy_type {0};
};

/// Extends snapshot type detection — clients can identify PvEvP snapshots.
constexpr ae::u16 kPvEvPSnapshotTag = 4;

/// PvEvPModifier adds AI enemy waves on top of any PvP base activity.
/// The base activity handles player-vs-player combat; the modifier
/// spawns and ticks AI enemies that attack all players.
template <typename BaseActivity>
class PvEvPModifier : public wish::core::ActivityModifier<BaseActivity> {
public:
    using wish::core::ActivityModifier<BaseActivity>::ActivityModifier;

    bool initialize(const wish::core::ActivityConfig& cfg) override {
        config_ = cfg;
        return this->base().initialize(cfg);
    }

    void tick(float dt) override {
        this->base().tick(dt);
        wave_timer_ += dt;
        if (wave_timer_ >= time_between_waves_) {
            spawn_wave();
            wave_timer_ = 0.0F;
            time_between_waves_ = std::max(10.0F, time_between_waves_ - 1.0F);
        }
        tick_enemies(dt);
    }

    wish::core::ActivityCategory category() const override {
        return wish::core::ActivityCategory::PvEvP;
    }

private:
    void spawn_wave() {
        ae::u8 count = static_cast<ae::u8>(3 + wave_number_ * 2);
        if (count > 16) count = 16;
        for (ae::u8 i = 0; i < count; ++i) {
            ModEnemyState e {};
            e.enemy_id = wave_number_ * 100 + i;
            e.position = Vec3 {
                (static_cast<float>(i) - count * 0.5F) * 2.0F,
                2.0F,
                -15.0F + static_cast<float>(wave_number_) * 0.5F
            };
            e.health = 100.0F + wave_number_ * 25.0F;
            e.alive = true;
            e.enemy_type = (i % 3 == 0) ? 1U : (i % 3 == 1) ? 2U : 0U;
            enemies_[i] = e;
        }
        enemy_count_ = count;
        wave_number_++;
    }

    void tick_enemies(float dt) {
        for (ae::u8 i = 0; i < enemy_count_; ++i) {
            if (!enemies_[i].alive) continue;
            float dx = -enemies_[i].position.x;
            float dz = -enemies_[i].position.z;
            float dist = std::sqrt(dx * dx + dz * dz);
            if (dist > 0.1F) {
                float speed = enemies_[i].enemy_type == 2 ? 4.0F : 2.0F;
                enemies_[i].position.x += (dx / dist) * speed * dt;
                enemies_[i].position.z += (dz / dist) * speed * dt;
            }
        }
    }

    wish::core::ActivityConfig config_;
    ModEnemyState enemies_[16] {};
    ae::u8 enemy_count_ {0};
    ae::u8 wave_number_ {1};
    float wave_timer_ {0.0F};
    float time_between_waves_ {30.0F};
};

}  // namespace ahamkara::game::activities
