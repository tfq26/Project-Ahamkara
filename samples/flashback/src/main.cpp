#include "ae/core/log.h"
#include "ae/core/math.h"
#include "ae/core/time.h"
#include "ae/platform/window.h"
#include "ae/render/debug_renderer.h"
#include "ae/runtime/application.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>

namespace {

constexpr float kPlayerHeight = 1.70F;
constexpr float kPlayerRadius = 0.32F;
constexpr float kMouseSensitivity = 0.08F;
constexpr float kLookKeySpeed = 92.0F;
constexpr float kGravity = 15.0F;
constexpr float kJumpVelocity = 6.4F;
constexpr float kWalkSpeed = 5.0F;
constexpr float kSprintSpeed = 7.0F;
constexpr float kShotSpeed = 36.0F;
constexpr int kMaxAmmo = 12;

enum class GameMode {
    StartMenu,
    Playing,
    Complete
};

struct LevelBox {
    ae::Vec3 min {};
    ae::Vec3 max {};
    float red {0.24F};
    float green {0.28F};
    float blue {0.34F};
};

struct Target {
    ae::Vec3 position {};
    bool alive {true};
    float hit_flash_seconds {0.0F};
};

struct ShotTrail {
    ae::Vec3 position {};
    ae::Vec3 velocity {};
    float life_seconds {0.0F};
};

struct Player {
    ae::Vec3 position {-8.0F, 0.35F, 0.0F};
    ae::Vec3 velocity {};
    float yaw_degrees {88.0F};
    float pitch_degrees {0.0F};
    bool on_ground {true};
};

[[nodiscard]] ae::render::Vec3 to_render_vec3(const ae::Vec3& value) {
    return {value.x, value.y, value.z};
}

[[nodiscard]] ae::Vec3 camera_forward(float yaw_degrees, float pitch_degrees) {
    const float yaw = ae::to_radians(yaw_degrees);
    const float pitch = ae::to_radians(pitch_degrees);
    const float cos_pitch = std::cos(pitch);
    return {
        std::sin(yaw) * cos_pitch,
        std::sin(pitch),
        std::cos(yaw) * cos_pitch,
    };
}

[[nodiscard]] ae::Vec3 horizontal_forward(float yaw_degrees) {
    const float yaw = ae::to_radians(yaw_degrees);
    return {std::sin(yaw), 0.0F, std::cos(yaw)};
}

[[nodiscard]] ae::Vec3 horizontal_right(float yaw_degrees) {
    const float yaw = ae::to_radians(yaw_degrees);
    return {std::cos(yaw), 0.0F, -std::sin(yaw)};
}

[[nodiscard]] bool overlaps_xz(const LevelBox& box, const ae::Vec3& position, float radius) {
    return position.x >= box.min.x - radius && position.x <= box.max.x + radius
        && position.z >= box.min.z - radius && position.z <= box.max.z + radius;
}

class FlashbackGame {
public:
    FlashbackGame() {
        reset_level();
    }

    void update(const ae::PlatformWindow& window, float delta_seconds) {
        const float dt = std::clamp(delta_seconds, 0.0F, 0.05F);

        if (window.is_key_pressed(ae::KeyCode::Escape)) {
            mode_ = GameMode::StartMenu;
            reset_level();
        }

        if (mode_ == GameMode::StartMenu) {
            if (window.is_key_pressed(ae::KeyCode::Enter)) {
                mode_ = GameMode::Playing;
                reset_level();
            }
            update_feedback(dt);
            previous_fire_down_ = is_fire_down(window);
            return;
        }

        if (mode_ == GameMode::Complete) {
            if (window.is_key_pressed(ae::KeyCode::Enter)) {
                mode_ = GameMode::StartMenu;
                reset_level();
            }
            update_feedback(dt);
            previous_fire_down_ = is_fire_down(window);
            return;
        }

        update_look(window, dt);
        update_movement(window, dt);
        update_weapon(window);
        update_feedback(dt);
        update_completion();

        previous_fire_down_ = is_fire_down(window);
    }

    [[nodiscard]] ae::render::DebugScene build_scene() {
        ae::render::DebugScene scene {};
        scene.draw_default_map = false;
        scene.always_day = true;
        scene.gamma = 1.05F;
        scene.camera_mode_name = "FLASHBACK";
        scene.player_position = to_render_vec3(player_.position);
        scene.player_height = kPlayerHeight;
        scene.player_yaw = ae::to_radians(player_.yaw_degrees);
        scene.player_health = 100.0F;
        scene.player_max_health = 100.0F;
        scene.ammo_current = static_cast<float>(ammo_);
        scene.ammo_max = static_cast<float>(kMaxAmmo);
        scene.hitmarker_time = hitmarker_seconds_ * 5.0F;
        scene.hitmarker_is_critical = false;
        scene.muzzle_flash_time = muzzle_flash_seconds_ * 10.0F;

        const ae::Vec3 eye = camera_position();
        const ae::Vec3 forward = camera_forward(player_.yaw_degrees, player_.pitch_degrees).normalized();
        scene.camera_position = to_render_vec3(eye);
        scene.camera_target = to_render_vec3(eye + forward * 18.0F);

        scene.level_box_count = static_cast<int>(level_boxes_.size());
        for (int i = 0; i < scene.level_box_count && i < 64; ++i) {
            const LevelBox& box = level_boxes_[static_cast<std::size_t>(i)];
            scene.level_boxes[i] = {
                to_render_vec3(box.min),
                to_render_vec3(box.max),
                box.red,
                box.green,
                box.blue,
            };
        }

        scene.dummy_count = static_cast<int>(targets_.size());
        for (int i = 0; i < scene.dummy_count && i < 16; ++i) {
            const Target& target = targets_[static_cast<std::size_t>(i)];
            scene.dummy_positions[i] = to_render_vec3(target.position);
            scene.dummy_yaws[i] = ae::to_radians(-90.0F);
            scene.dummy_alive[i] = target.alive;
            scene.dummy_recently_hit[i] = target.hit_flash_seconds > 0.0F;
        }

        scene.projectile_count = 0;
        for (const ShotTrail& shot : shots_) {
            if (shot.life_seconds <= 0.0F || scene.projectile_count >= 64) {
                continue;
            }
            scene.projectile_positions[scene.projectile_count] = to_render_vec3(shot.position);
            ++scene.projectile_count;
        }

        if (mode_ == GameMode::StartMenu) {
            scene.hud_visible = false;
            scene.show_player_marker = false;
            scene.show_crosshair = false;
            scene.camera_position = {-9.0F, 5.0F, -9.0F};
            scene.camera_target = {3.0F, 1.4F, 0.0F};
            scene.overlay_title = "FLASHBACK";
            scene.overlay_body = "CLEAR THE RANGE. CROSS THE PLATFORMS. REACH THE EXIT.";
            scene.overlay_hint = "ENTER TO START";
            return scene;
        }

        if (mode_ == GameMode::Complete) {
            scene.hud_visible = false;
            scene.show_player_marker = true;
            scene.show_crosshair = false;
            scene.overlay_title = "LEVEL COMPLETE";
            scene.overlay_body = "TARGETS DOWN. ROUTE CLEARED. MEMORY LOCK RECOVERED.";
            scene.overlay_hint = "ENTER TO RETURN TO START";
            return scene;
        }

        scene.hud_visible = true;
        scene.show_player_marker = false;
        scene.show_crosshair = true;
        std::snprintf(
            objective_buffer_,
            sizeof(objective_buffer_),
            "TARGETS %d/%d  |  REACH EXIT",
            destroyed_target_count(),
            static_cast<int>(targets_.size()));
        scene.objective_text = objective_buffer_;
        return scene;
    }

private:
    void reset_level() {
        player_ = {};
        ammo_ = kMaxAmmo;
        hitmarker_seconds_ = 0.0F;
        muzzle_flash_seconds_ = 0.0F;
        shots_ = {};
        previous_fire_down_ = false;

        targets_ = {{
            {{-2.2F, 0.45F, -1.6F}, true, 0.0F},
            {{0.0F, 1.48F, 1.1F}, true, 0.0F},
            {{4.3F, 2.48F, -0.9F}, true, 0.0F},
            {{7.2F, 3.12F, 1.3F}, true, 0.0F},
            {{10.2F, 3.42F, 0.0F}, true, 0.0F},
        }};
    }

    void update_look(const ae::PlatformWindow& window, float dt) {
        const ae::MouseState mouse = window.mouse_state();
        player_.yaw_degrees += mouse.delta_x * kMouseSensitivity;
        player_.pitch_degrees -= mouse.delta_y * kMouseSensitivity;

        if (window.is_key_down(ae::KeyCode::Left)) {
            player_.yaw_degrees -= kLookKeySpeed * dt;
        }
        if (window.is_key_down(ae::KeyCode::Right)) {
            player_.yaw_degrees += kLookKeySpeed * dt;
        }
        if (window.is_key_down(ae::KeyCode::Up)) {
            player_.pitch_degrees += kLookKeySpeed * dt;
        }
        if (window.is_key_down(ae::KeyCode::Down)) {
            player_.pitch_degrees -= kLookKeySpeed * dt;
        }

        player_.yaw_degrees = ae::wrap_degrees(player_.yaw_degrees);
        player_.pitch_degrees = std::clamp(player_.pitch_degrees, -72.0F, 72.0F);
    }

    void update_movement(const ae::PlatformWindow& window, float dt) {
        ae::Vec2 move_axis {};
        if (window.is_key_down(ae::KeyCode::W)) {
            move_axis.y += 1.0F;
        }
        if (window.is_key_down(ae::KeyCode::S)) {
            move_axis.y -= 1.0F;
        }
        if (window.is_key_down(ae::KeyCode::D)) {
            move_axis.x += 1.0F;
        }
        if (window.is_key_down(ae::KeyCode::A)) {
            move_axis.x -= 1.0F;
        }
        if (move_axis.length_squared() > 1.0F) {
            move_axis = move_axis.normalized();
        }

        const float speed = window.is_key_down(ae::KeyCode::LeftShift) ? kSprintSpeed : kWalkSpeed;
        const ae::Vec3 desired_velocity =
            horizontal_forward(player_.yaw_degrees) * (move_axis.y * speed)
            + horizontal_right(player_.yaw_degrees) * (move_axis.x * speed);

        player_.velocity.x = desired_velocity.x;
        player_.velocity.z = desired_velocity.z;

        if (window.is_key_pressed(ae::KeyCode::Space) && player_.on_ground) {
            player_.velocity.y = kJumpVelocity;
            player_.on_ground = false;
        }

        const float previous_y = player_.position.y;
        player_.velocity.y -= kGravity * dt;
        player_.position += player_.velocity * dt;

        resolve_floor_collision(previous_y);

        if (player_.position.y < -8.0F || std::fabs(player_.position.x) > 24.0F || std::fabs(player_.position.z) > 18.0F) {
            reset_player_to_start();
        }
    }

    void update_weapon(const ae::PlatformWindow& window) {
        if (window.is_key_pressed(ae::KeyCode::R)) {
            ammo_ = kMaxAmmo;
        }

        const bool fire_down = is_fire_down(window);
        if (!fire_down || previous_fire_down_ || ammo_ <= 0) {
            return;
        }

        --ammo_;
        muzzle_flash_seconds_ = 0.12F;
        const ae::Vec3 origin = camera_position();
        const ae::Vec3 direction = camera_forward(player_.yaw_degrees, player_.pitch_degrees).normalized();
        spawn_shot(origin, direction);

        int hit_index = -1;
        float nearest_t = 1000.0F;
        for (int i = 0; i < static_cast<int>(targets_.size()); ++i) {
            Target& target = targets_[static_cast<std::size_t>(i)];
            if (!target.alive) {
                continue;
            }

            const ae::Vec3 center = target.position + ae::Vec3 {0.0F, 0.78F, 0.0F};
            float t = 0.0F;
            if (ray_hits_sphere(origin, direction, center, 0.58F, t) && t < nearest_t) {
                nearest_t = t;
                hit_index = i;
            }
        }

        if (hit_index >= 0) {
            Target& target = targets_[static_cast<std::size_t>(hit_index)];
            target.alive = false;
            target.hit_flash_seconds = 0.18F;
            hitmarker_seconds_ = 0.20F;
        }
    }

    void update_feedback(float dt) {
        hitmarker_seconds_ = std::max(0.0F, hitmarker_seconds_ - dt);
        muzzle_flash_seconds_ = std::max(0.0F, muzzle_flash_seconds_ - dt);

        for (Target& target : targets_) {
            target.hit_flash_seconds = std::max(0.0F, target.hit_flash_seconds - dt);
        }

        for (ShotTrail& shot : shots_) {
            if (shot.life_seconds <= 0.0F) {
                continue;
            }
            shot.position += shot.velocity * dt;
            shot.life_seconds -= dt;
        }
    }

    void update_completion() {
        if (destroyed_target_count() != static_cast<int>(targets_.size())) {
            return;
        }

        const LevelBox& endpoint = level_boxes_.back();
        if (overlaps_xz(endpoint, player_.position, kPlayerRadius)
            && player_.position.y >= endpoint.max.y - 0.10F) {
            mode_ = GameMode::Complete;
        }
    }

    void resolve_floor_collision(float previous_y) {
        player_.on_ground = false;
        float best_floor = -1000.0F;

        for (const LevelBox& box : level_boxes_) {
            if (!overlaps_xz(box, player_.position, kPlayerRadius)) {
                continue;
            }
            const float floor_y = box.max.y;
            if (floor_y < best_floor) {
                continue;
            }
            if (previous_y + 0.08F >= floor_y && player_.position.y <= floor_y && player_.velocity.y <= 0.0F) {
                best_floor = floor_y;
            }
        }

        if (best_floor > -999.0F) {
            player_.position.y = best_floor;
            player_.velocity.y = 0.0F;
            player_.on_ground = true;
        }
    }

    void reset_player_to_start() {
        player_.position = {-8.0F, 0.35F, 0.0F};
        player_.velocity = {};
        player_.yaw_degrees = 88.0F;
        player_.pitch_degrees = 0.0F;
        player_.on_ground = true;
    }

    void spawn_shot(const ae::Vec3& origin, const ae::Vec3& direction) {
        for (ShotTrail& shot : shots_) {
            if (shot.life_seconds > 0.0F) {
                continue;
            }
            shot.position = origin + direction * 0.75F;
            shot.velocity = direction * kShotSpeed;
            shot.life_seconds = 0.18F;
            return;
        }
    }

    [[nodiscard]] bool ray_hits_sphere(
        const ae::Vec3& origin,
        const ae::Vec3& direction,
        const ae::Vec3& center,
        float radius,
        float& out_t) const {
        const ae::Vec3 oc = origin - center;
        const float b = ae::dot(oc, direction);
        const float c = ae::dot(oc, oc) - radius * radius;
        const float discriminant = b * b - c;
        if (discriminant < 0.0F) {
            return false;
        }

        const float sqrt_discriminant = std::sqrt(discriminant);
        const float t = -b - sqrt_discriminant;
        if (t <= 0.0F || t > 80.0F) {
            return false;
        }

        out_t = t;
        return true;
    }

    [[nodiscard]] ae::Vec3 camera_position() const {
        return player_.position + ae::Vec3 {0.0F, 1.48F, 0.0F};
    }

    [[nodiscard]] bool is_fire_down(const ae::PlatformWindow& window) const {
        const ae::MouseState mouse = window.mouse_state();
        return mouse.button_down[static_cast<int>(ae::MouseButton::Left)]
            || window.is_key_down(ae::KeyCode::F);
    }

    [[nodiscard]] int destroyed_target_count() const {
        int destroyed = 0;
        for (const Target& target : targets_) {
            if (!target.alive) {
                ++destroyed;
            }
        }
        return destroyed;
    }

    GameMode mode_ {GameMode::StartMenu};
    Player player_ {};
    int ammo_ {kMaxAmmo};
    bool previous_fire_down_ {false};
    float hitmarker_seconds_ {0.0F};
    float muzzle_flash_seconds_ {0.0F};
    char objective_buffer_[96] {};

    std::array<LevelBox, 7> level_boxes_ {{
        {{-10.0F, -0.05F, -3.2F}, {-4.0F, 0.35F, 3.2F}, 0.22F, 0.26F, 0.32F},
        {{-3.5F, -0.05F, -1.2F}, {-1.8F, 0.35F, 1.2F}, 0.28F, 0.31F, 0.38F},
        {{-0.8F, 0.95F, -1.7F}, {1.8F, 1.25F, 1.7F}, 0.18F, 0.34F, 0.36F},
        {{3.0F, 1.95F, -1.7F}, {5.8F, 2.25F, 1.7F}, 0.20F, 0.38F, 0.34F},
        {{6.8F, 2.55F, -1.8F}, {8.8F, 2.85F, 1.8F}, 0.26F, 0.31F, 0.42F},
        {{9.4F, 3.05F, -2.4F}, {12.2F, 3.35F, 2.4F}, 0.34F, 0.27F, 0.42F},
        {{10.0F, 3.35F, -0.8F}, {11.6F, 3.95F, 0.8F}, 0.86F, 0.62F, 0.20F},
    }};
    std::array<Target, 5> targets_ {};
    std::array<ShotTrail, 16> shots_ {};
};

}  // namespace

int main() {
    ae::WindowConfig window_config {};
    window_config.title = "Flashback";
    window_config.width = 1280;
    window_config.height = 720;
    window_config.create_opengl_context = true;

    std::unique_ptr<ae::PlatformWindow> window;
    try {
        window = ae::PlatformWindow::create(window_config);
    } catch (const std::exception& ex) {
        ae::log_error(ex.what());
        return EXIT_FAILURE;
    }

    ae::Application application(ae::RuntimeMode::Client);
    application.start();

    ae::render::DebugRenderer renderer;
    if (!renderer.initialize(*window)) {
        ae::log_error("Flashback failed to initialize the debug renderer.");
        return EXIT_FAILURE;
    }

    FlashbackGame game;
    ae::log_info("Flashback started. Enter starts, W/A/S/D moves, arrows or mouse look, Space jumps, left click/F fires, R reloads.");

    double last_time = ae::now_seconds();
    while (application.is_running() && window->poll_events()) {
        const double current_time = ae::now_seconds();
        const float delta_seconds = static_cast<float>(current_time - last_time);
        last_time = current_time;

        game.update(*window, delta_seconds);
        ae::render::DebugScene scene = game.build_scene();
        renderer.render(scene);
    }

    renderer.shutdown();
    application.shutdown();
    return EXIT_SUCCESS;
}
