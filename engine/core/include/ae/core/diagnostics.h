#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ae {

/**
 * @brief System information snapshot (OS, CPU, memory, etc.)
 */
struct SystemInfo {
    std::string os_name;          ///< e.g. "macOS 14.5"
    std::string cpu_brand;        ///< e.g. "Apple M3 Pro"
    int cpu_core_count{0};        ///< Logical core count
    std::uint64_t total_ram_bytes{0};  ///< Total physical RAM in bytes
    std::string gpu_renderer;     ///< GPU renderer string (if available)
    std::string engine_version;   ///< Engine version/build identifier
};

/**
 * @brief Collect system information for diagnostics.
 *
 * Reads /proc/cpuinfo (Linux), sysctl (macOS), or environment variables.
 * Returns best-effort data; any unavailable fields remain at default values.
 */
[[nodiscard]] SystemInfo collect_system_info();

/**
 * @brief Write a full diagnostic bundle to the output directory.
 *
 * A bundle is a timestamped directory containing:
 *   - system_info.txt       — OS, CPU, RAM, GPU info
 *   - config_dump.txt       — All registered ConfigVar values (if ConfigRegistry available)
 *   - log_tail.txt          — Recent log lines (last 500 lines from ahakara.log)
 *   - crash_summary.txt     — Latest crash dump summary (if any exist)
 *
 * @param output_dir  Directory to write the bundle into.
 * @return Path to the created bundle directory, or empty on failure.
 */
[[nodiscard]] std::filesystem::path write_diagnostic_bundle(
    const std::filesystem::path& output_dir = "diagnostics");

/**
 * @brief Collect the tail of the engine log file.
 *
 * Reads the last `max_lines` lines from the log file at `log_path`.
 *
 * @return Vector of log lines, oldest first.
 */
[[nodiscard]] std::vector<std::string> collect_log_tail(
    const std::filesystem::path& log_path = "logs/ahamkara.log",
    int max_lines = 500);

/**
 * @brief Collect all crash dumps and produce a summary string.
 */
[[nodiscard]] std::string collect_crash_summary(
    const std::filesystem::path& crash_dir = "crashes");

/**
 * @brief List existing diagnostic bundles sorted by modification time
 *        (most recent first).
 */
[[nodiscard]] std::vector<std::filesystem::path> list_diagnostic_bundles(
    const std::filesystem::path& output_dir = "diagnostics");

}  // namespace ae
