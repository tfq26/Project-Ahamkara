#include "ahamkara/client/debug_inspector.h"
#include "ahamkara/client/debug_scene_bridge.h"
#include "ahamkara/game/net_types.h"

#include "ae/core/log.h"

#include <imgui.h>
#include <cstdio>
#include <string>

#define AE_LOG_CATEGORY "Inspector"

namespace ahamkara::client {

namespace {
const char* movement_state_name(ahamkara::game::MovementState state) {
    using ahamkara::game::MovementState;
    switch (state) {
        case MovementState::Idle:      return "Idle";
        case MovementState::Walking:   return "Walking";
        case MovementState::Sprinting: return "Sprinting";
        case MovementState::Sliding:   return "Sliding";
        case MovementState::Jumping:   return "Jumping";
        case MovementState::OnLadder:  return "OnLadder";
        case MovementState::LedgeGrab: return "LedgeGrab";
        case MovementState::Mantling:  return "Mantling";
        default: return "Unknown";
    }
}
} // anonymous namespace

void DebugInspector::render(const ClientSimulationSnapshot& snapshot) {
    if (!visible_) return;

    ImGui::SetNextWindowSize(ImVec2(420, 500), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(20, 40), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Debug Inspector##inspector", &visible_,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    // Tabs
    if (ImGui::BeginTabBar("InspectorTabs")) {
        if (ImGui::BeginTabItem("Entity")) {
            render_entity_inspector(snapshot);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Performance")) {
            render_performance_panel(snapshot);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Weapon")) {
            render_weapon_inspector(snapshot);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void DebugInspector::render_entity_inspector(const ClientSimulationSnapshot& snap) {
    ImGui::Text("Player State");
    ImGui::Separator();

    ImGui::Text("Position:  %.2f, %.2f, %.2f",
                snap.player_position.x, snap.player_position.y, snap.player_position.z);
    ImGui::Text("Velocity:  %.2f, %.2f, %.2f",
                snap.player_state.velocity.x, snap.player_state.velocity.y, snap.player_state.velocity.z);
    ImGui::Text("Yaw:       %.1f", snap.player_state.yaw);
    ImGui::Text("Health:    %.0f / %.0f", snap.player_state.health, 100.0F);
    ImGui::Text("Movement:  %s", movement_state_name(static_cast<ahamkara::game::MovementState>(snap.player_state.movement_state)));
    ImGui::Text("Match Time: %.1f  Phase: %d", snap.match_time, snap.match_phase);

    ImGui::Separator();
    ImGui::Text("Match State");
    if (snap.match_over) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "  MATCH OVER");
    }
    ImGui::Text("Score:  Red %u  Blue %u", snap.team_score_red, snap.team_score_blue);
    ImGui::Text("K/D:    %u / %u", snap.player_kills, snap.player_deaths);

    ImGui::Separator();
    ImGui::Text("Target Dummies (%d)", snap.dummy_count);
    for (int i = 0; i < snap.dummy_count && i < ClientSimulationSnapshot::kMaxDummies; ++i) {
        const auto& d = snap.dummies[i];
        ImGui::Text("  [%d] pos=(%.1f,%.1f,%.1f) alive=%s",
                    i, d.position.x, d.position.y, d.position.z,
                    d.alive ? "yes" : "no");
    }

    ImGui::Separator();
    ImGui::Text("Projectiles (%d)", snap.projectile_count);
    for (int i = 0; i < snap.projectile_count && i < 8; ++i) {
        const auto& p = snap.projectiles[i];
        ImGui::Text("  [%d] pos=(%.1f,%.1f,%.1f) alive=%s",
                    i, p.position.x, p.position.y, p.position.z,
                    p.alive ? "yes" : "no");
    }

    ImGui::Separator();
    ImGui::Text("Particles: %d  Decals: %d", snap.particle_count, snap.decal_count);
    ImGui::Text("Damage Numbers: %d", snap.damage_number_count);
}

void DebugInspector::render_performance_panel(const ClientSimulationSnapshot& /*snap*/) {
    ImGui::Text("Frame Timing");
    ImGui::Separator();

    // Note: per-frame timing data comes from DebugFrontendState, not the snapshot.
    // This panel shows what's available; detailed timing is in the F3 metrics overlay.
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
        "Detailed performance metrics are displayed via the F3 overlay.");

    ImGui::Separator();
    ImGui::Text("Snapshot Info");
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
        "The inspector reads from the current client simulation snapshot.");
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
        "Frame timing and FPS are available in the F3 metrics overlay.");
}

void DebugInspector::render_weapon_inspector(const ClientSimulationSnapshot& snap) {
    ImGui::Text("Weapon State");
    ImGui::Separator();

    ImGui::Text("Active Weapon: %d", snap.weapon_index);
    ImGui::Text("Ammo:  %.0f / %.0f  (Reserve: %d)",
                snap.ammo_current, snap.ammo_max, snap.reserve_ammo);

    ImGui::Separator();
    ImGui::Text("Ability State");
    ImGui::Text("Grenades:  %d (cooldown: %.1f)", snap.grenade_count, snap.grenade_cooldown);
    ImGui::Text("Special:   %s (cooldown: %.1f)", snap.special_available ? "ready" : "cooldown", snap.special_cooldown);
    ImGui::Text("Artifact:  cooldown=%.1f", snap.artifact_cooldown);
    ImGui::Text("Ultimate:  charge=%.1f%s", snap.ultimate_charge, snap.ultimate_ready ? " READY" : "");

    ImGui::Separator();
    ImGui::Text("Sensory");
    ImGui::Text("Hitmarker:  %.2f%s", snap.hitmarker_time, snap.hitmarker_is_critical ? " (critical)" : "");
    ImGui::Text("Muzzle Flash: %.2f", snap.muzzle_flash_time);
}

}  // namespace ahamkara::client
