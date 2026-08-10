#pragma once

#include "ahamkara/client/debug_scene_bridge.h"
#include "ahamkara/game/net_types.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ahamkara::client {

/// Development-only bridge between the local game client and the MCP server.
///
/// The bridge uses an atomic file queue rather than a network listener. This
/// keeps the first integration local to the developer workstation and avoids
/// exposing a game-control port to other machines. The source file is only
/// compiled when AHAMKARA_ENABLE_GAME_MCP is enabled.
class GameMcpBridge final {
  public:
    struct Config {
        std::filesystem::path root;
        std::string token;
        std::size_t max_commands_per_frame {1};
    };

    static std::optional<Config> config_from_environment();

    explicit GameMcpBridge(Config config);
    ~GameMcpBridge();

    GameMcpBridge(const GameMcpBridge&) = delete;
    GameMcpBridge& operator=(const GameMcpBridge&) = delete;

    bool start();
    void stop();

    /// Reads at most one authenticated command for this frame.
    [[nodiscard]] bool poll_input(ahamkara::game::PlayerInputCommand& out);

    /// Publishes a structured observation for the external MCP server.
    void publish_snapshot(const ClientSimulationSnapshot& snapshot,
                          std::uint64_t frame_index,
                          float elapsed_seconds,
                          double fps);

    /// Returns true when the authenticated MCP client requested a frame.
    [[nodiscard]] bool capture_requested() const;

    /// Writes the current framebuffer as a vertically-corrected PPM image.
    void write_frame_ppm(int width, int height, const std::vector<std::uint8_t>& rgba);

    [[nodiscard]] bool running() const {
        return running_;
    }

  private:
    bool read_command_file(const std::filesystem::path& path,
                           ahamkara::game::PlayerInputCommand& out,
                           int& duration_ticks);
    void write_state_file(const ClientSimulationSnapshot& snapshot,
                          std::uint64_t frame_index,
                          float elapsed_seconds,
                          double fps) const;

    Config config_;
    std::filesystem::path commands_dir_;
    std::filesystem::path state_path_;
    std::filesystem::path ready_path_;
    std::filesystem::path capture_request_path_;
    std::filesystem::path frame_path_;
    ahamkara::game::PlayerInputCommand active_command_ {};
    int active_ticks_remaining_ {0};
    std::uint32_t next_sequence_ {1};
    bool running_ {false};
};

} // namespace ahamkara::client
