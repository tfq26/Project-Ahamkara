#include "ahamkara/client/threaded_local_runtime.h"

#include <algorithm>
#include <chrono>
#include <thread>

namespace ahamkara::client {

void ThreadedLocalRuntime::ThreadSafeInputProvider::set_input(const ahamkara::game::PlayerInputCommand& command) {
    std::lock_guard<std::mutex> lock(mutex_);
    command_.look_delta.x += command.look_delta.x;
    command_.look_delta.y += command.look_delta.y;

    command_.move_axis = command.move_axis;
    command_.jump_pressed = command_.jump_pressed || command.jump_pressed;
    command_.crouch_held = command.crouch_held;
    command_.sprint_held = command.sprint_held;
    command_.slide_pressed = command_.slide_pressed || command.slide_pressed;
    command_.fire_held = command.fire_held;
    command_.reload_pressed = command_.reload_pressed || command.reload_pressed;
    command_.ability_pressed = command_.ability_pressed || command.ability_pressed;
}

ahamkara::game::PlayerInputCommand ThreadedLocalRuntime::ThreadSafeInputProvider::gather_input(float) {
    std::lock_guard<std::mutex> lock(mutex_);
    ahamkara::game::PlayerInputCommand command = command_;

    command_.jump_pressed = false;
    command_.slide_pressed = false;
    command_.reload_pressed = false;
    command_.ability_pressed = false;
    command_.look_delta = {0.0F, 0.0F};

    return command;
}

ThreadedLocalRuntime::ThreadedLocalRuntime()
    : simulation_([this]() {
          auto provider = std::make_unique<ThreadSafeInputProvider>();
          input_provider_view_ = provider.get();
          return provider;
      }())
    , sim_step_seconds_(simulation_.get_fixed_step_seconds()) {
}

ThreadedLocalRuntime::~ThreadedLocalRuntime() {
    stop();
}

void ThreadedLocalRuntime::start() {
    running_ = true;
    sim_thread_ = std::thread(&ThreadedLocalRuntime::sim_loop, this);
}

void ThreadedLocalRuntime::stop() {
    running_ = false;
    if (sim_thread_.joinable()) {
        sim_thread_.join();
    }
}

void ThreadedLocalRuntime::submit_input(const ahamkara::game::PlayerInputCommand& command) {
    if (input_provider_view_ != nullptr) {
        input_provider_view_->set_input(command);
    }
}

void ThreadedLocalRuntime::set_paused(bool paused) {
    paused_ = paused;
}

void ThreadedLocalRuntime::set_colliders(const ahamkara::game::ColliderBox* colliders, std::size_t count) {
    std::lock_guard<std::mutex> lock(sim_mutex_);
    simulation_.set_colliders(colliders, count);
}

void ThreadedLocalRuntime::set_audio_player(ahamkara::game::IAudioPlayer* player) {
    std::lock_guard<std::mutex> lock(sim_mutex_);
    simulation_.set_audio_player(player);
}

bool ThreadedLocalRuntime::load_level(const std::string& path) {
    std::lock_guard<std::mutex> lock(sim_mutex_);
    return simulation_.load_level(path);
}

void ThreadedLocalRuntime::restart_match() {
    std::lock_guard<std::mutex> lock(sim_mutex_);
    simulation_.restart_match();
    std::lock_guard<std::mutex> snap_lock(snapshot_mutex_);
    prev_snapshot_ = {};
    curr_snapshot_ = {};
    last_tick_time_ = std::chrono::steady_clock::now();
}

void ThreadedLocalRuntime::get_snapshots(
    ClientSimulationSnapshot& out_previous,
    ClientSimulationSnapshot& out_current,
    float& out_alpha) {
    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    out_previous = prev_snapshot_;
    out_current = curr_snapshot_;

    const auto now = std::chrono::steady_clock::now();
    const double time_since_last_tick = std::chrono::duration<double>(now - last_tick_time_).count();
    out_alpha = static_cast<float>(std::clamp(time_since_last_tick / sim_step_seconds_, 0.0, 1.0));
}

ClientSimulationSnapshot ThreadedLocalRuntime::build_snapshot_locked() const {
    ClientSimulationSnapshot snapshot;
    snapshot.player_state = simulation_.get_player_state();
    snapshot.player_height = simulation_.get_player_visual_height();
    snapshot.player_position = simulation_.get_interpolated_player_position(1.0F);
    snapshot.camera_anchor = simulation_.get_interpolated_camera_anchor(1.0F);
    snapshot.ammo_current = static_cast<float>(simulation_.get_ammo_current());
    snapshot.ammo_max = static_cast<float>(simulation_.get_ammo_max());
    snapshot.weapon_index = simulation_.get_active_weapon_index();
    snapshot.reserve_ammo = simulation_.get_reserve_ammo();
    snapshot.hitmarker_time = simulation_.get_hitmarker_time();
    snapshot.hitmarker_is_critical = simulation_.get_hitmarker_is_critical();
    snapshot.muzzle_flash_time = simulation_.get_muzzle_flash_time();

    snapshot.damage_number_count =
        std::min(simulation_.get_damage_number_count(), ClientSimulationSnapshot::kMaxDamageNumbers);
    for (int i = 0; i < snapshot.damage_number_count; ++i) {
        snapshot.damage_numbers[i] = simulation_.get_damage_numbers()[i];
    }

    snapshot.dummy_count = std::min(simulation_.get_dummy_count(), ClientSimulationSnapshot::kMaxDummies);
    for (int i = 0; i < snapshot.dummy_count; ++i) {
        snapshot.dummies[i] = simulation_.get_interpolated_dummy(i, 1.0F);
        snapshot.enemy_health[i] = simulation_.get_dummies()[i].health;
        snapshot.enemy_max_health[i] = 150.0F;
    }

    snapshot.projectile_count =
        std::min(simulation_.get_projectile_count(), ClientSimulationSnapshot::kMaxProjectiles);
    for (int i = 0; i < snapshot.projectile_count; ++i) {
        snapshot.projectiles[i] = simulation_.get_projectiles()[i];
    }

    snapshot.particle_count = 0;
    snapshot.decal_count = 0;

    snapshot.player_kills = simulation_.get_player_kills();
    snapshot.player_deaths = simulation_.get_player_deaths();
    snapshot.match_time = simulation_.get_match_time();
    snapshot.match_phase = simulation_.get_match_phase();
    snapshot.match_over = simulation_.is_match_over();
    snapshot.player_alive = simulation_.is_player_alive();
    snapshot.damage_feedback_timer = simulation_.get_damage_feedback_timer();
    snapshot.team_score_red = 0;
    snapshot.team_score_blue = simulation_.get_player_kills();

    return snapshot;
}

void ThreadedLocalRuntime::sim_loop() {
    const auto tick_duration = std::chrono::duration<double>(sim_step_seconds_);
    auto next_tick_time = std::chrono::steady_clock::now();

    while (running_) {
        if (paused_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            next_tick_time = std::chrono::steady_clock::now();
            continue;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= next_tick_time) {
            {
                std::lock_guard<std::mutex> lock(sim_mutex_);
                simulation_.tick(static_cast<float>(sim_step_seconds_));
            }

            ClientSimulationSnapshot new_snapshot;
            {
                std::lock_guard<std::mutex> lock(sim_mutex_);
                new_snapshot = build_snapshot_locked();
            }

            {
                std::lock_guard<std::mutex> lock(snapshot_mutex_);
                prev_snapshot_ = curr_snapshot_;
                curr_snapshot_ = new_snapshot;
                last_tick_time_ = now;
            }

            next_tick_time += std::chrono::duration_cast<std::chrono::steady_clock::duration>(tick_duration);
            if (now > next_tick_time +
                    std::chrono::duration_cast<std::chrono::steady_clock::duration>(tick_duration * 5.0)) {
                next_tick_time = now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(tick_duration);
            }
        } else {
            const double sleep_duration = std::chrono::duration<double>(next_tick_time - now).count();
            if (sleep_duration > 0.001) {
                std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(sleep_duration * 1000.0)));
            } else {
                std::this_thread::yield();
            }
        }
    }
}

}  // namespace ahamkara::client
