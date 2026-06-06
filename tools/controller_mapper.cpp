#include "ae/platform/window.h"
#include "ae/render/font_atlas.h"
#include "ahamkara/client/controller_bindings.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <cstdio>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>

namespace {

constexpr const char* kBindingsRelativePath = "client/config/controller_bindings.cfg";

struct BindingEntry {
    const char* name;
    ae::GamepadInputCode* code;
};

std::filesystem::path find_project_root() {
    return std::filesystem::current_path();
}

std::filesystem::path bindings_path() {
    return find_project_root() / kBindingsRelativePath;
}

std::string format_code(ae::GamepadInputCode code) {
    if (code == ae::kInvalidGamepadInputCode) {
        return "None";
    }

    unsigned int type = code & 0xFFFF0000U;
    unsigned int index = code & 0x0000FFFFU;

    if (type == ae::kGamepadButtonCodeBase) {
        switch (index) {
            case 0: return "Button A / Cross";
            case 1: return "Button B / Circle";
            case 2: return "Button X / Square";
            case 3: return "Button Y / Triangle";
            case 4: return "Left Bumper";
            case 5: return "Right Bumper";
            case 6: return "Back / Share";
            case 7: return "Start / Options";
            case 8: return "Guide";
            case 9: return "Left Stick Press";
            case 10: return "Right Stick Press";
            case 11: return "D-Pad Up";
            case 12: return "D-Pad Right";
            case 13: return "D-Pad Down";
            case 14: return "D-Pad Left";
            default: {
                char buffer[32] {};
                std::snprintf(buffer, sizeof(buffer), "Button %u", index);
                return buffer;
            }
        }
    } else if (type == ae::kGamepadAxisPositiveCodeBase) {
        switch (index) {
            case 0: return "Left Stick Right";
            case 1: return "Left Stick Down";
            case 2: return "Right Stick Right";
            case 3: return "Right Stick Down";
            case 4: return "Left Trigger (+)";
            case 5: return "Right Trigger (+)";
            default: {
                char buffer[32] {};
                std::snprintf(buffer, sizeof(buffer), "Axis %u (+)", index);
                return buffer;
            }
        }
    } else if (type == ae::kGamepadAxisNegativeCodeBase) {
        switch (index) {
            case 0: return "Left Stick Left";
            case 1: return "Left Stick Up";
            case 2: return "Right Stick Left";
            case 3: return "Right Stick Up";
            case 4: return "Left Trigger (-)";
            case 5: return "Right Trigger (-)";
            default: {
                char buffer[32] {};
                std::snprintf(buffer, sizeof(buffer), "Axis %u (-)", index);
                return buffer;
            }
        }
    } else if (type == ae::kGamepadHatCodeBase) {
        std::string hat_str = "Hat";
        if (index & 1) hat_str += " Up";
        if (index & 2) hat_str += " Right";
        if (index & 4) hat_str += " Down";
        if (index & 8) hat_str += " Left";
        return hat_str;
    }

    char buffer[32] {};
    std::snprintf(buffer, sizeof(buffer), "0x%08X", code);
    return buffer;
}

void draw_quad(float x, float y, float w, float h) {
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
}

void draw_panel(float x, float y, float w, float h, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    draw_quad(x, y, w, h);
    glEnd();
}

void draw_text(ae::render::FontAtlas& atlas, float x, float y, float scale, std::string_view text, float r, float g, float b) {
    glColor3f(r, g, b);
    atlas.draw_text(x, y, scale, text);
}

}  // namespace

int main() {
    ae::WindowConfig config {};
    config.title = "Ahamkara Controller Mapper";
    config.width = 960;
    config.height = 720;
    config.create_opengl_context = true;

    std::unique_ptr<ae::PlatformWindow> window = ae::PlatformWindow::create(config);
    auto* native = static_cast<GLFWwindow*>(window->native_handle());
    glfwMakeContextCurrent(native);

    ae::render::FontAtlas font_atlas;
    font_atlas.initialize_default();

    ahamkara::client::ControllerBindings bindings {};
    (void)bindings.load_from_file(bindings_path().string());

    BindingEntry entries[] = {
        {"Jump", &bindings.jump},
        {"Crouch", &bindings.crouch},
        {"Slide", &bindings.slide},
        {"Reload", &bindings.reload},
        {"Sprint", &bindings.sprint},
        {"Ability", &bindings.ability},
        {"Metrics", &bindings.metrics},
        {"Menu", &bindings.menu},
        {"TogglePerspective", &bindings.toggle_perspective},
        {"Fire", &bindings.fire},
    };

    int selected_index = 0;
    bool capture_mode = false;
    std::string status = "Use Up/Down to select, Enter to capture, S to save, Esc to quit.";
    double last_modified_time = -10.0;
    int last_modified_index = -1;
    ae::GamepadInputCode persistent_last_pressed_code = ae::kInvalidGamepadInputCode;

    while (window->poll_events()) {
        if (window->is_key_pressed(ae::KeyCode::Escape)) {
            break;
        }

        const ae::GamepadDebugState& debug_state = window->gamepad_debug_state();
        if (debug_state.last_pressed_code != ae::kInvalidGamepadInputCode) {
            persistent_last_pressed_code = debug_state.last_pressed_code;
        }

        if (!capture_mode) {
            if (window->is_key_pressed(ae::KeyCode::Up)) {
                selected_index = (selected_index + static_cast<int>(std::size(entries)) - 1) % static_cast<int>(std::size(entries));
            }
            if (window->is_key_pressed(ae::KeyCode::Down)) {
                selected_index = (selected_index + 1) % static_cast<int>(std::size(entries));
            }
            if (window->is_key_pressed(ae::KeyCode::Enter)) {
                capture_mode = true;
                status = std::string("Press a controller button/trigger for ") + entries[selected_index].name;
            }
            if (window->is_key_pressed(ae::KeyCode::S)) {
                if (bindings.save_to_file(bindings_path().string())) {
                    status = "Saved controller bindings.";
                    last_modified_time = glfwGetTime();
                    last_modified_index = -2; // special value for saved flash
                } else {
                    status = "Failed to save controller bindings.";
                }
            }
        } else {
            if (debug_state.last_pressed_code != ae::kInvalidGamepadInputCode) {
                *entries[selected_index].code = debug_state.last_pressed_code;
                status = std::string(entries[selected_index].name) + " bound to " + format_code(debug_state.last_pressed_code);
                last_modified_index = selected_index;
                last_modified_time = glfwGetTime();
                capture_mode = false;
            }
        }

        auto* native = static_cast<GLFWwindow*>(window->native_handle());
        glfwMakeContextCurrent(native);

        int fb_w = 1;
        int fb_h = 1;
        glfwGetFramebufferSize(native, &fb_w, &fb_h);
        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.05F, 0.07F, 0.11F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, static_cast<double>(fb_w), static_cast<double>(fb_h), 0.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        draw_panel(24.0F, 24.0F, static_cast<float>(fb_w) - 48.0F, static_cast<float>(fb_h) - 48.0F, 0.03F, 0.05F, 0.08F, 0.95F);
        draw_panel(24.0F, 24.0F, static_cast<float>(fb_w) - 48.0F, 52.0F, 0.10F, 0.18F, 0.30F, 1.0F);

        draw_text(font_atlas, 44.0F, 40.0F, 1.6F, "AHAMKARA CONTROLLER MAPPER", 0.98F, 0.99F, 1.0F);

        std::ostringstream info;
        info << "Controller: " << (debug_state.connected ? debug_state.name : "Not connected")
             << " | Mapping: " << (debug_state.standardized_mapping ? "Standardized" : "Fallback");
        draw_text(font_atlas, 44.0F, 96.0F, 1.2F, info.str(), 0.84F, 0.88F, 0.93F);

        std::ostringstream raw;
        raw << "Last input code: " << format_code(persistent_last_pressed_code);
        draw_text(font_atlas, 44.0F, 126.0F, 1.2F, raw.str(), 0.96F, 0.84F, 0.16F);

        float row_y = 180.0F;
        const double current_time = glfwGetTime();
        for (int i = 0; i < static_cast<int>(std::size(entries)); ++i) {
            if (i == last_modified_index && current_time - last_modified_time < 0.6) {
                // Flash green feedback
                float pulse = static_cast<float>(1.0 - (current_time - last_modified_time) / 0.6);
                draw_panel(40.0F, row_y - 6.0F, static_cast<float>(fb_w) - 80.0F, 30.0F, 
                           0.08F * (1.0F - pulse) + 0.1F * pulse, 
                           0.18F * (1.0F - pulse) + 0.6F * pulse, 
                           0.30F * (1.0F - pulse) + 0.2F * pulse, 
                           0.9F);
            } else if (i == selected_index) {
                if (capture_mode) {
                    // Pulsing amber/orange to show active capture mode
                    float pulse = static_cast<float>(0.5 + 0.5 * std::sin(current_time * 10.0));
                    draw_panel(40.0F, row_y - 6.0F, static_cast<float>(fb_w) - 80.0F, 30.0F, 
                               0.35F, 0.18F + 0.05F * pulse, 0.05F, 0.9F);
                } else {
                    // Standard selection highlight
                    draw_panel(40.0F, row_y - 6.0F, static_cast<float>(fb_w) - 80.0F, 30.0F, 0.14F, 0.24F, 0.38F, 0.9F);
                }
            }

            // Draw text with context-aware colors
            float text_r = 0.98F;
            float text_g = 0.99F;
            float text_b = 1.0F;
            if (i == selected_index && capture_mode) {
                text_r = 1.0F;
                text_g = 0.8F;
                text_b = 0.3F;
            } else if (i == last_modified_index && current_time - last_modified_time < 0.6) {
                text_r = 0.4F;
                text_g = 1.0F;
                text_b = 0.5F;
            }

            draw_text(font_atlas, 56.0F, row_y, 1.2F, entries[i].name, text_r, text_g, text_b);
            
            float val_r = 0.80F;
            float val_g = 0.84F;
            float val_b = 0.88F;
            if (i == selected_index && capture_mode) {
                val_r = 1.0F; val_g = 0.8F; val_b = 0.3F;
            } else if (i == last_modified_index && current_time - last_modified_time < 0.6) {
                val_r = 0.4F; val_g = 1.0F; val_b = 0.5F;
            }
            
            draw_text(font_atlas, 360.0F, row_y, 1.2F, format_code(*entries[i].code), val_r, val_g, val_b);
            row_y += 34.0F;
        }

        // Highlight status bar
        float status_r = 0.78F;
        float status_g = 0.86F;
        float status_b = 0.96F;
        if (capture_mode) {
            status_r = 1.0F;
            status_g = 0.6F;
            status_b = 0.1F;
        } else if (current_time - last_modified_time < 0.8) {
            status_r = 0.2F;
            status_g = 0.9F;
            status_b = 0.4F;
        }

        draw_text(font_atlas, 44.0F, static_cast<float>(fb_h) - 84.0F, 1.2F, status, status_r, status_g, status_b);
        draw_text(font_atlas, 44.0F, static_cast<float>(fb_h) - 52.0F, 1.0F, "UP/DOWN SELECT  ENTER CAPTURE  S SAVE  ESC EXIT", 0.62F, 0.68F, 0.76F);

        glfwSwapBuffers(native);
    }

    font_atlas.shutdown();
    return 0;
}
