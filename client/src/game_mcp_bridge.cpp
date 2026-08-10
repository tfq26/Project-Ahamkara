#include "ahamkara/client/game_mcp_bridge.h"

#include "ae/core/log.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <vector>

#define AE_LOG_CATEGORY "GameMcp"

namespace ahamkara::client {
namespace {

bool enabled_value(const char* value) {
    return value != nullptr && (std::string_view(value) == "1" ||
                                std::string_view(value) == "true" ||
                                std::string_view(value) == "yes");
}

bool parse_float(std::string_view value, float& out) {
    std::string owned(value);
    char* end = nullptr;
    const float parsed = std::strtof(owned.c_str(), &end);
    if (end == owned.c_str() || *end != '\0')
        return false;
    out = parsed;
    return true;
}

bool parse_int(std::string_view value, int& out) {
    int parsed = 0;
    const auto* begin = value.data();
    const auto* end = begin + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc {} || result.ptr != end)
        return false;
    out = parsed;
    return true;
}

bool parse_bool(std::string_view value, bool& out) {
    if (value == "1" || value == "true" || value == "yes") {
        out = true;
        return true;
    }
    if (value == "0" || value == "false" || value == "no") {
        out = false;
        return true;
    }
    return false;
}

std::string json_number(float value) {
    std::ostringstream stream;
    stream << std::setprecision(7) << value;
    return stream.str();
}

void write_vec3(std::ostream& out, const ahamkara::game::Vec3& value) {
    out << "{\"x\":" << json_number(value.x)
        << ",\"y\":" << json_number(value.y)
        << ",\"z\":" << json_number(value.z) << "}";
}

void write_atomic(const std::filesystem::path& destination, const std::string& contents) {
    const auto temporary = destination.string() + ".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream)
            return;
        stream << contents;
        stream.flush();
        if (!stream)
            return;
    }
    std::error_code error;
    std::filesystem::rename(temporary, destination, error);
    if (error) {
        std::filesystem::remove(destination, error);
        error.clear();
        std::filesystem::rename(temporary, destination, error);
    }
}

} // namespace

std::optional<GameMcpBridge::Config> GameMcpBridge::config_from_environment() {
    if (!enabled_value(std::getenv("AHAMKARA_GAME_MCP_ENABLED")))
        return std::nullopt;

    const char* root = std::getenv("AHAMKARA_GAME_MCP_ROOT");
    const char* token = std::getenv("AHAMKARA_GAME_MCP_TOKEN");
    if (root == nullptr || root[0] == '\0' || token == nullptr || token[0] == '\0') {
        ae::log_warning_cat(AE_LOG_CATEGORY,
                            "Game MCP requested but AHAMKARA_GAME_MCP_ROOT or token is missing; disabled.");
        return std::nullopt;
    }

    Config config;
    config.root = std::filesystem::absolute(root);
    config.token = token;
    return config;
}

GameMcpBridge::GameMcpBridge(Config config)
    : config_(std::move(config)), commands_dir_(config_.root / "commands"), state_path_(config_.root / "state.json"), ready_path_(config_.root / "ready.json"), capture_request_path_(config_.root / "capture.request"), frame_path_(config_.root / "frame.ppm") {}

GameMcpBridge::~GameMcpBridge() {
    stop();
}

bool GameMcpBridge::start() {
    if (running_)
        return true;

    std::error_code error;
    std::filesystem::create_directories(commands_dir_, error);
    if (error) {
        ae::log_error_cat(AE_LOG_CATEGORY,
                          "Failed to create Game MCP command directory: " + error.message());
        return false;
    }

    write_atomic(ready_path_, "{\"protocol\":1,\"transport\":\"local-files\"}\n");
    running_ = true;
    ae::log_info_cat(AE_LOG_CATEGORY,
                     "Game MCP bridge enabled using local command/state files under " + config_.root.string());
    return true;
}

void GameMcpBridge::stop() {
    if (!running_)
        return;
    std::error_code error;
    std::filesystem::remove(ready_path_, error);
    running_ = false;
}

bool GameMcpBridge::read_command_file(const std::filesystem::path& path,
                                      ahamkara::game::PlayerInputCommand& out,
                                      int& duration_ticks) {
    std::ifstream stream(path);
    if (!stream)
        return false;

    std::string field;
    std::string supplied_token;
    out = {};
    duration_ticks = 1;
    while (stream >> field) {
        const auto separator = field.find('=');
        if (separator == std::string::npos)
            continue;
        const std::string_view key(field.data(), separator);
        const std::string_view value(field.data() + separator + 1,
                                     field.size() - separator - 1);
        if (key == "token")
            supplied_token = std::string(value);
        else if (key == "move_x") {
            if (!parse_float(value, out.move_axis.x))
                return false;
        } else if (key == "move_y") {
            if (!parse_float(value, out.move_axis.y))
                return false;
        } else if (key == "look_x") {
            if (!parse_float(value, out.look_delta.x))
                return false;
        } else if (key == "look_y") {
            if (!parse_float(value, out.look_delta.y))
                return false;
        } else if (key == "weapon") {
            int parsed = 0;
            if (!parse_int(value, parsed))
                return false;
            out.weapon_slot = static_cast<ae::u8>(std::clamp(parsed, 0, 255));
        } else if (key == "duration_ticks") {
            if (!parse_int(value, duration_ticks))
                return false;
        } else if (key == "jump") {
            if (!parse_bool(value, out.jump_pressed))
                return false;
        } else if (key == "crouch") {
            if (!parse_bool(value, out.crouch_held))
                return false;
        } else if (key == "sprint") {
            if (!parse_bool(value, out.sprint_held))
                return false;
        } else if (key == "slide") {
            if (!parse_bool(value, out.slide_pressed))
                return false;
        } else if (key == "fire") {
            if (!parse_bool(value, out.fire_held))
                return false;
        } else if (key == "reload") {
            if (!parse_bool(value, out.reload_pressed))
                return false;
        } else if (key == "ability") {
            if (!parse_bool(value, out.ability_pressed))
                return false;
        } else if (key == "interact") {
            if (!parse_bool(value, out.interact_pressed))
                return false;
        } else if (key == "aim") {
            if (!parse_bool(value, out.aim_held))
                return false;
        }
    }

    if (supplied_token != config_.token || duration_ticks < 1 || duration_ticks > 600)
        return false;
    out.sequence = next_sequence_;
    out.client_tick = next_sequence_;
    ++next_sequence_;
    return true;
}

bool GameMcpBridge::poll_input(ahamkara::game::PlayerInputCommand& out) {
    if (!running_)
        return false;

    if (active_ticks_remaining_ > 0) {
        out = active_command_;
        --active_ticks_remaining_;
        return true;
    }

    std::vector<std::filesystem::path> commands;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(commands_dir_, error)) {
        if (entry.path().extension() == ".cmd")
            commands.push_back(entry.path());
    }
    if (error || commands.empty())
        return false;
    std::sort(commands.begin(), commands.end());

    int duration_ticks = 1;
    if (!read_command_file(commands.front(), out, duration_ticks)) {
        std::filesystem::remove(commands.front(), error);
        return false;
    }
    std::filesystem::remove(commands.front(), error);
    active_command_ = out;
    active_ticks_remaining_ = duration_ticks - 1;
    return true;
}

void GameMcpBridge::publish_snapshot(const ClientSimulationSnapshot& snapshot,
                                     std::uint64_t frame_index,
                                     float elapsed_seconds,
                                     double fps) {
    if (!running_)
        return;

    std::ostringstream state;
    state << "{\"protocol\":1,\"frame\":" << frame_index
          << ",\"elapsed_seconds\":" << json_number(elapsed_seconds)
          << ",\"fps\":" << std::setprecision(7) << fps
          << ",\"player\":{\"position\":";
    write_vec3(state, snapshot.player_state.position);
    state << ",\"velocity\":";
    write_vec3(state, snapshot.player_state.velocity);
    state << ",\"yaw\":" << json_number(snapshot.player_state.yaw)
          << ",\"health\":" << json_number(snapshot.player_state.health)
          << ",\"shield\":" << json_number(snapshot.player_state.shield)
          << ",\"alive\":" << (snapshot.player_alive ? "true" : "false") << "}"
          << ",\"weapon\":{\"index\":" << snapshot.weapon_index
          << ",\"ammo\":" << json_number(snapshot.ammo_current)
          << ",\"max_ammo\":" << json_number(snapshot.ammo_max)
          << ",\"reserve\":" << snapshot.reserve_ammo << "}"
          << ",\"match\":{\"time\":" << json_number(snapshot.match_time)
          << ",\"phase\":" << static_cast<int>(snapshot.match_phase)
          << ",\"over\":" << (snapshot.match_over ? "true" : "false") << "}"
          << ",\"dummies\":[";
    for (int i = 0; i < snapshot.dummy_count; ++i) {
        if (i != 0)
            state << ',';
        const auto& dummy = snapshot.dummies[i];
        state << "{\"id\":" << dummy.dummy_id << ",\"position\":";
        write_vec3(state, dummy.position);
        state << ",\"health\":" << json_number(dummy.health)
              << ",\"alive\":" << (dummy.alive ? "true" : "false") << "}";
    }
    state << "],\"projectiles\":" << snapshot.projectile_count
          << ",\"events\":{\"reload_requests\":0,\"hitmarker\":"
          << (snapshot.hitmarker_time > 0.0F ? "true" : "false")
          << ",\"muzzle_flash\":" << (snapshot.muzzle_flash_time > 0.0F ? "true" : "false")
          << "}}\n";
    write_atomic(state_path_, state.str());
}

bool GameMcpBridge::capture_requested() const {
    if (!running_)
        return false;
    std::ifstream stream(capture_request_path_);
    std::string supplied_token;
    std::getline(stream, supplied_token);
    return supplied_token == config_.token;
}

void GameMcpBridge::write_frame_ppm(int width, int height,
                                    const std::vector<std::uint8_t>& rgba) {
    if (!running_ || width <= 0 || height <= 0 ||
        rgba.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U) {
        return;
    }

    const auto temporary = frame_path_.string() + ".tmp";
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    if (!stream)
        return;
    stream << "P6\n"
           << width << " " << height << "\n255\n";
    for (int y = height - 1; y >= 0; --y) {
        const auto row = static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 4U;
        for (int x = 0; x < width; ++x) {
            const auto pixel = row + static_cast<std::size_t>(x) * 4U;
            stream.put(static_cast<char>(rgba[pixel + 0]));
            stream.put(static_cast<char>(rgba[pixel + 1]));
            stream.put(static_cast<char>(rgba[pixel + 2]));
        }
    }
    stream.flush();
    if (!stream)
        return;
    stream.close();

    std::error_code error;
    std::filesystem::rename(temporary, frame_path_, error);
    if (error) {
        std::filesystem::remove(frame_path_, error);
        error.clear();
        std::filesystem::rename(temporary, frame_path_, error);
    }
    std::filesystem::remove(capture_request_path_, error);
}

} // namespace ahamkara::client
