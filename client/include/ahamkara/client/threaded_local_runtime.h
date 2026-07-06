#pragma once

#include "ahamkara/client/debug_scene_bridge.h"
#include "ahamkara/client/local_play.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace ahamkara::client {

class ThreadedLocalRuntime {
public:
    ThreadedLocalRuntime();
    ~ThreadedLocalRuntime();

    ThreadedLocalRuntime(const ThreadedLocalRuntime&) = delete;
    ThreadedLocalRuntime& operator=(const ThreadedLocalRuntime&) = delete;
    ThreadedLocalRuntime(ThreadedLocalRuntime&&) = delete;
    ThreadedLocalRuntime& operator=(ThreadedLocalRuntime&&) = delete;

    void start();
    void stop();

    void submit_input(const ahamkara::game::PlayerInputCommand& command);
    void set_paused(bool paused);
    void set_colliders(const ahamkara::game::ColliderBox* colliders, std::size_t count);
    void set_interaction_targets(const ahamkara::game::InteractionTargetDefinition* targets, std::size_t count);
    void set_audio_player(ahamkara::game::IAudioPlayer* player);
    bool load_level(const std::string& path);
    void get_snapshots(ClientSimulationSnapshot& out_previous, ClientSimulationSnapshot& out_current, float& out_alpha);
    void restart_match();
    [[nodiscard]] ahamkara::game::ReplicatedPlayerState get_player_state() const;
    [[nodiscard]] int get_interaction_success_count() const;
    [[nodiscard]] int get_reload_request_count() const;
    [[nodiscard]] int get_ability_use_count() const;

private:
    class ThreadSafeInputProvider final : public IInputProvider {
    public:
        void set_input(const ahamkara::game::PlayerInputCommand& command);
        [[nodiscard]] ahamkara::game::PlayerInputCommand gather_input(float delta_seconds) override;
        [[nodiscard]] bool finished() const override;

    private:
        std::mutex mutex_;
        ahamkara::game::PlayerInputCommand command_ {};
        bool finished_ {false};
    };

    [[nodiscard]] ClientSimulationSnapshot build_snapshot_locked() const;
    void sim_loop();

    ThreadSafeInputProvider* input_provider_view_ {nullptr};
    LocalPlaySimulation simulation_;
    std::thread sim_thread_;
    std::atomic<bool> running_ {false};
    std::atomic<bool> paused_ {false};

    std::mutex sim_mutex_;
    std::mutex snapshot_mutex_;

    ClientSimulationSnapshot prev_snapshot_ {};
    ClientSimulationSnapshot curr_snapshot_ {};
    double sim_step_seconds_ {1.0 / 60.0};
    std::chrono::steady_clock::time_point last_tick_time_ {std::chrono::steady_clock::now()};
};

}  // namespace ahamkara::client
