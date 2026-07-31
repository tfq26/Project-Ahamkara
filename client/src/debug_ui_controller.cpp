#include "ahamkara/client/debug_ui_controller.h"

#include "ae/core/log.h"
#include "ae/input/input_map.h"
#include "ae/platform/gamepad.h"
#include "ae/platform/window.h"
#include "ae/render/debug_renderer.h"
#include "ahamkara/game/weapon_registry.h"
#include "imgui.h"

#include <cstdio>
#include <cmath>
#include <algorithm>

#define AE_LOG_CATEGORY "Client"

namespace ahamkara::client {

static constexpr ImVec4 kAccent      {0.20F, 0.55F, 0.90F, 1.00F};
static constexpr ImVec4 kSuccess     {0.20F, 0.80F, 0.40F, 1.00F};
static constexpr ImVec4 kDanger      {0.85F, 0.25F, 0.25F, 1.00F};
static constexpr ImVec4 kWarning     {0.95F, 0.65F, 0.10F, 1.00F};
static constexpr ImVec4 kTextBright  {0.95F, 0.96F, 0.98F, 1.00F};
static constexpr ImVec4 kTextDim     {0.55F, 0.58F, 0.65F, 1.00F};
static constexpr ImVec4 kBgPanel     {0.07F, 0.09F, 0.14F, 0.94F};

static void TextCenteredInPanel(const char* text, float panel_w) {
    float tw = ImGui::CalcTextSize(text).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (panel_w - tw) * 0.5F);
    ImGui::TextUnformatted(text);
}

static bool MenuButton(const char* label, const ImVec2& size, bool primary = false) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(24, 10));

    if (primary) {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.85F));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.30F, 0.65F, 1.00F, 0.95F));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(kAccent.x, kAccent.y, kAccent.z, 1.00F));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.18F, 0.20F, 0.28F, 0.70F));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.24F, 0.27F, 0.38F, 0.85F));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.20F, 0.23F, 0.33F, 0.95F));
    }

    bool clicked = ImGui::Button(label, size);

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
    return clicked;
}

DebugUiController::DebugUiController(ClientMenuState& menu_state,
                                     const ClientConfig& client_config)
    : menu_state_(menu_state) {
    load_from_config(client_config);
}

bool DebugUiController::visible() const { return menu_state_.visible(); }
int DebugUiController::active_menu_tab() const { return static_cast<int>(menu_state_.screen()); }

void DebugUiController::load_from_config(const ClientConfig& client_config) {
    auto& s = menu_state_.menu_state();
    s.pending_width        = client_config.window_width;
    s.pending_height       = client_config.window_height;
    s.pending_fullscreen   = client_config.fullscreen;
    s.pending_gamma        = client_config.gamma;
    s.pending_mouse_sens   = client_config.mouse_sensitivity;
    s.pending_master_vol   = client_config.audio.master_volume;
    s.pending_sfx_vol      = client_config.audio.sfx_volume;
    s.pending_audio_enabled = client_config.audio.enabled;
}

void DebugUiController::apply_to_config(ClientConfig& client_config) const {
    const auto& s = menu_state_.menu_state();
    client_config.window_width          = s.pending_width;
    client_config.window_height         = s.pending_height;
    client_config.fullscreen            = s.pending_fullscreen;
    client_config.gamma                 = s.pending_gamma;
    client_config.mouse_sensitivity     = s.pending_mouse_sens;
    client_config.audio.master_volume   = s.pending_master_vol;
    client_config.audio.sfx_volume      = s.pending_sfx_vol;
    client_config.audio.enabled         = s.pending_audio_enabled;
}

DebugUiActions DebugUiController::handle_menu_toggle(bool toggle_requested, ClientConfig& client_config) {
    DebugUiActions actions;
    if (!toggle_requested) return actions;

    bool changed = menu_state_.toggle_menu();
    if (changed) {
        if (!menu_state_.visible()) {
            apply_to_config(client_config);
            actions.config_applied = true;
        }
    }
    return actions;
}

DebugUiActions DebugUiController::render(
    ae::input::InputMap& input_map,
    ae::PlatformWindow& window,
    const ae::GamepadState& gamepad,
    const ClientSimulationSnapshot& current_snapshot,
    const ae::render::DebugScene& render_scene,
    ClientConfig& client_config) {

    DebugUiActions actions;
    auto& io = ImGui::GetIO();

    input_map.poll(&window, gamepad);
    if (input_map.get(ae::input::InputAction::Scoreboard).pressed) {
        show_scoreboard_ = !show_scoreboard_;
    }

    // =========================================================================
    // Scoreboard
    // =========================================================================
    if (show_scoreboard_ && !menu_state_.visible()) {
        float sw = 480.0F, sh = 340.0F;
        ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - sw) * 0.5F, 60.0F));
        ImGui::SetNextWindowSize(ImVec2(sw, sh));

        ImGui::PushStyleColor(ImGuiCol_WindowBg, kBgPanel);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.12F, 0.15F, 0.22F, 0.80F));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0F);

        if (ImGui::Begin("Scoreboard", &show_scoreboard_,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {

            // Accent bar
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImGui::GetWindowPos(),
                ImVec2(ImGui::GetWindowPos().x + sw, ImGui::GetWindowPos().y + 3.0F),
                ImGui::ColorConvertFloat4ToU32(kAccent));

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
            ImGui::SetWindowFontScale(1.3F);
            TextCenteredInPanel("MATCH SCOREBOARD", sw);
            ImGui::SetWindowFontScale(1.0F);
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Player row — centered with data columns
            if (ImGui::BeginTable("SBTable", 4,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 24.0F);
                ImGui::TableSetupColumn("Player");
                ImGui::TableSetupColumn("Kills", ImGuiTableColumnFlags_WidthFixed, 60.0F);
                ImGui::TableSetupColumn("Deaths", ImGuiTableColumnFlags_WidthFixed, 60.0F);
                ImGui::TableHeadersRow();

                // Player row
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(kAccent, ">");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(kTextBright, "YOU");
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%u", static_cast<unsigned>(current_snapshot.player_kills));
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%u", static_cast<unsigned>(current_snapshot.player_deaths));

                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::Separator();

            // Summary stats
            float kd = 0.0F;
            if (current_snapshot.player_deaths > 0) {
                kd = static_cast<float>(current_snapshot.player_kills) /
                     static_cast<float>(current_snapshot.player_deaths);
            }

            if (ImGui::BeginTable("SBSummary", 3,
                ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("K/D");
                ImGui::TableSetupColumn("Time");
                ImGui::TableSetupColumn("Status");
                ImGui::TableHeadersRow();

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(kWarning, "%.2f", kd);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.0f s", render_scene.match_time);
                ImGui::TableSetColumnIndex(2);
                ImGui::TextColored(current_snapshot.match_over ? kDanger : kSuccess, "%s",
                    current_snapshot.match_over ? "MATCH OVER" : "LIVE");

                ImGui::EndTable();
            }
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
        ImGui::End();
    }

    // =========================================================================
    // Death overlay
    // =========================================================================
    if (!current_snapshot.player_alive && current_snapshot.player_deaths > 0) {
        float dw = 360.0F, dh = 110.0F;
        ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - dw) * 0.5F,
                                       io.DisplaySize.y * 0.32F));
        ImGui::SetNextWindowSize(ImVec2(dw, dh));

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.55F));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0F);

        if (ImGui::Begin("DeathOverlay", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav)) {

            // Pulsing red accent line
            float pulse = 0.4F + 0.2F * (float)std::sin(ImGui::GetTime() * 3.0);
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImGui::GetWindowPos(),
                ImVec2(ImGui::GetWindowPos().x + dw, ImGui::GetWindowPos().y + 3.0F),
                ImGui::ColorConvertFloat4ToU32(ImVec4(kDanger.x, kDanger.y, kDanger.z, pulse)));

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, kDanger);
            ImGui::SetWindowFontScale(2.2F);
            TextCenteredInPanel("ELIMINATED", dw);
            ImGui::SetWindowFontScale(1.0F);
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
            ImGui::SetWindowFontScale(0.9F);
            TextCenteredInPanel("Respawning...", dw);
            ImGui::SetWindowFontScale(1.0F);
            ImGui::PopStyleColor();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::End();
    }

    // =========================================================================
    // Match End screen
    // =========================================================================
    if (current_snapshot.match_over && !menu_state_.visible()) {
        // Full-screen dim background
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.60F));

        if (ImGui::Begin("MatchEndBg", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus)) {

            // Center panel
            float pw = 460.0F, ph = 360.0F;
            ImGui::SetCursorPos(ImVec2((io.DisplaySize.x - pw) * 0.5F,
                                        (io.DisplaySize.y - ph) * 0.5F));

            ImGui::PushStyleColor(ImGuiCol_ChildBg, kBgPanel);
            ImGui::BeginChild("MatchEndPanel", ImVec2(pw, ph),
                ImGuiChildFlags_Border);

            // Victory accent bar
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImGui::GetCursorScreenPos(),
                ImVec2(ImGui::GetCursorScreenPos().x + pw,
                       ImGui::GetCursorScreenPos().y + 4.0F),
                ImGui::ColorConvertFloat4ToU32(kSuccess));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 14.0F);

            // Title
            ImGui::PushStyleColor(ImGuiCol_Text, kSuccess);
            ImGui::SetWindowFontScale(2.4F);
            TextCenteredInPanel("MISSION COMPLETE", pw);
            ImGui::SetWindowFontScale(1.0F);
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
            TextCenteredInPanel("All hostiles neutralized", pw);
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Stats — 2x3 grid
            if (ImGui::BeginTable("EndStats", 2, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Stat");
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 100.0F);

                auto row = [&](const char* label, const char* fmt, ...) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextColored(kTextDim, "%s", label);
                    ImGui::TableSetColumnIndex(1);
                    char buf[64];
                    va_list args;
                    va_start(args, fmt);
                    vsnprintf(buf, sizeof(buf), fmt, args);
                    va_end(args);
                    ImGui::TextColored(kTextBright, "%s", buf);
                };

                float kd = current_snapshot.player_deaths > 0
                    ? static_cast<float>(current_snapshot.player_kills) /
                      static_cast<float>(current_snapshot.player_deaths) : 0.0F;

                row("Enemies Killed", "%u", static_cast<unsigned>(current_snapshot.player_kills));
                row("Deaths",         "%u", static_cast<unsigned>(current_snapshot.player_deaths));
                row("K/D Ratio",      "%.2f", kd);
                row("Completion Time", "%.0f s", static_cast<double>(render_scene.match_time));
                row("Accuracy",       "--");
                row("Headshots",      "--");

                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            float btn_w = 220.0F;
            float cx = (pw - btn_w) * 0.5F;

            ImGui::SetCursorPosX(cx);
            if (MenuButton("PLAY AGAIN", ImVec2(btn_w, 40.0F), true)) {
                actions.restart_match = true;
            }

            ImGui::Spacing();
            ImGui::SetCursorPosX(cx);
            if (MenuButton("QUIT TO MENU", ImVec2(btn_w, 34.0F))) {
                menu_state_.show_pause();
            }

            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
        ImGui::PopStyleColor();
        ImGui::End();
    }

    // =========================================================================
    // Menu Screen routing
    // =========================================================================
    if (!menu_state_.visible()) return actions;

    auto& ui = menu_state_.menu_state();
    bool quit_to_menu = false;
    switch (menu_state_.screen()) {
    case ae::ui::MenuScreen::MainMenu:
        ae::ui::render_main_menu(ui, &actions.quit_application);
        if (!ui.visible || ui.screen == ae::ui::MenuScreen::None) {
            menu_state_.start_gameplay();
        } else if (ui.screen == ae::ui::MenuScreen::Settings) {
            menu_state_.open_settings();
        }
        break;
    case ae::ui::MenuScreen::PauseOverlay:
        ae::ui::render_pause_overlay(ui, &quit_to_menu);
        if (!ui.visible || ui.screen == ae::ui::MenuScreen::None) {
            menu_state_.resume_gameplay();
        } else if (ui.screen == ae::ui::MenuScreen::Settings) {
            menu_state_.open_settings();
        } else if (ui.screen == ae::ui::MenuScreen::Character) {
            menu_state_.open_character();
        } else if (ui.screen == ae::ui::MenuScreen::Docs) {
            menu_state_.open_docs();
        }
        break;
    case ae::ui::MenuScreen::Settings:
        ae::ui::render_settings(ui);
        if (ui.screen != ae::ui::MenuScreen::Settings) {
            menu_state_.back_from_settings();
        }
        break;
    case ae::ui::MenuScreen::Docs:
        ae::ui::render_docs_viewer(ui);
        if (ui.screen != ae::ui::MenuScreen::Docs) {
            menu_state_.back_to_pause();
        }
        break;
    case ae::ui::MenuScreen::Character: {
        int ammo = static_cast<int>(current_snapshot.ammo_current);
        int max_ammo = static_cast<int>(current_snapshot.ammo_max);
        float hp = current_snapshot.player_state.health;
        const char* weapon_name = ahamkara::game::weapon_name(current_snapshot.weapon_index);
        if (ae::ui::render_character_sheet(&hp, 100.0F, ammo, max_ammo,
            weapon_name, current_snapshot.reserve_ammo)) {
            menu_state_.back_to_pause();
        }
        break;
    }
    default: break;
    }

    if (quit_to_menu) {
        menu_state_.show_main_menu();
    }

    if (!menu_state_.visible() || menu_state_.screen() == ae::ui::MenuScreen::None) {
        apply_to_config(client_config);
        actions.config_applied = true;
    }

    return actions;
}

}  // namespace ahamkara::client
