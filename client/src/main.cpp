#include "ahamkara/client/client_config.h"
#include "ahamkara/client/controller_bindings.h"
#include "ae/core/log.h"

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

// ── Forward declarations ─────────────────────────────────────────────────────

int run_local_client(
    const ahamkara::client::ClientConfig& client_config,
    const ahamkara::client::ControllerBindings& controller_bindings);

int run_windowed_client(const ahamkara::client::ClientConfig& client_config);
int run_sandbox_client();
int run_network_client(const std::string& server_ip);

// ── Path resolution helpers ──────────────────────────────────────────────────

namespace {

constexpr const char* kClientConfigRelativePath = "client/config/ahamkara.cfg";
constexpr const char* kControllerBindingsRelativePath = "client/config/controller_bindings.cfg";

std::optional<std::filesystem::path> current_executable_path(const char* argv0) {
#if defined(__APPLE__)
    uint32_t buffer_size = 0;
    _NSGetExecutablePath(nullptr, &buffer_size);
    if (buffer_size > 0) {
        std::vector<char> buffer(buffer_size);
        if (_NSGetExecutablePath(buffer.data(), &buffer_size) == 0) {
            return std::filesystem::weakly_canonical(buffer.data());
        }
    }
#elif defined(__linux__)
    char buffer[PATH_MAX] {};
    const ssize_t bytes_written = ::readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (bytes_written > 0) {
        buffer[bytes_written] = '\0';
        return std::filesystem::weakly_canonical(buffer);
    }
#endif

    if (argv0 != nullptr && argv0[0] != '\0') {
        return std::filesystem::absolute(std::filesystem::path(argv0));
    }

    return std::nullopt;
}

std::optional<std::filesystem::path> find_client_config_path(const char* executable_path) {
    const auto cwd_candidate = std::filesystem::path(kClientConfigRelativePath);
    if (std::filesystem::exists(cwd_candidate)) {
        return cwd_candidate;
    }

    if (const auto resolved_executable_path = current_executable_path(executable_path);
        resolved_executable_path.has_value()) {
        const auto executable_dir = resolved_executable_path->parent_path();
        const auto repo_relative_candidate =
            executable_dir / ".." / ".." / ".." / kClientConfigRelativePath;
        if (std::filesystem::exists(repo_relative_candidate)) {
            return repo_relative_candidate.lexically_normal();
        }
    }

    return std::nullopt;
}

std::optional<std::filesystem::path> find_controller_bindings_path(const char* executable_path) {
    const auto cwd_candidate = std::filesystem::path(kControllerBindingsRelativePath);
    if (std::filesystem::exists(cwd_candidate)) {
        return cwd_candidate;
    }

    if (const auto resolved_executable_path = current_executable_path(executable_path);
        resolved_executable_path.has_value()) {
        const auto executable_dir = resolved_executable_path->parent_path();
        const auto repo_relative_candidate =
            executable_dir / ".." / ".." / ".." / kControllerBindingsRelativePath;
        if (std::filesystem::exists(repo_relative_candidate)) {
            return repo_relative_candidate.lexically_normal();
        }
    }

    return std::nullopt;
}

}  // namespace

// ── Entry point ──────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    ahamkara::client::ClientConfig client_config {};
    ahamkara::client::ControllerBindings controller_bindings {};
    if (const auto config_path = find_client_config_path(argc > 0 ? argv[0] : nullptr); config_path.has_value()) {
        (void)client_config.load_from_file(config_path->string());
    } else {
        ae::log_info("No client config file found, using defaults.");
    }
    if (const auto bindings_path = find_controller_bindings_path(argc > 0 ? argv[0] : nullptr); bindings_path.has_value()) {
        (void)controller_bindings.load_from_file(bindings_path->string());
    } else {
        ae::log_info("No controller bindings file found, using defaults.");
    }

    if (argc > 1 && (std::string(argv[1]) == "--local" || std::string(argv[1]) == "--debug-view")) {
        return run_local_client(client_config, controller_bindings);
    }

    if (argc > 1 && std::string(argv[1]) == "--window") {
        return run_windowed_client(client_config);
    }

    if (argc > 1 && std::string(argv[1]) == "--sandbox") {
        return run_sandbox_client();
    }

    const std::string server_ip = argc > 1 ? argv[1] : client_config.server_ip;
    return run_network_client(server_ip);
}
