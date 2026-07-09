#include "ae/core/diagnostics.h"
#include "ae/core/config.h"
#include "ae/core/crash_handler.h"
#include "ae/core/log.h"
#include "ae/core/time.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>

#ifdef __APPLE__
#include <sys/sysctl.h>
#include <mach/mach.h>
#endif

#ifdef __linux__
#include <fstream>
#include <unistd.h>
#endif

#define AE_LOG_CATEGORY "Diagnostics"

namespace ae {

// ===================================================================
// collect_system_info
// ===================================================================

SystemInfo collect_system_info() {
    SystemInfo info;

#ifdef __APPLE__
    // OS version
    char os_vers[256] = {};
    std::size_t os_len = sizeof(os_vers);
    if (sysctlbyname("kern.osrelease", os_vers, &os_len, nullptr, 0) == 0) {
        info.os_name = "macOS ";
        // Try the product version string
        char prod_vers[64] = {};
        std::size_t prod_len = sizeof(prod_vers);
        if (sysctlbyname("kern.osproductversion", prod_vers, &prod_len, nullptr, 0) == 0) {
            info.os_name += prod_vers;
        } else {
            info.os_name += os_vers;
        }
    } else {
        info.os_name = "macOS (unknown version)";
    }

    // CPU brand
    char cpu_brand[256] = {};
    std::size_t cpu_len = sizeof(cpu_brand);
    if (sysctlbyname("machdep.cpu.brand_string", cpu_brand, &cpu_len, nullptr, 0) == 0) {
        info.cpu_brand = cpu_brand;
    } else {
        // Apple Silicon doesn't have machdep.cpu.brand_string; try hw.model
        char hw_model[128] = {};
        std::size_t hw_len = sizeof(hw_model);
        if (sysctlbyname("hw.model", hw_model, &hw_len, nullptr, 0) == 0) {
            info.cpu_brand = "Apple " + std::string(hw_model);
        } else {
            info.cpu_brand = "Unknown CPU";
        }
    }

    // Core count
    int core_count = 0;
    std::size_t core_len = sizeof(core_count);
    if (sysctlbyname("hw.logicalcpu", &core_count, &core_len, nullptr, 0) == 0) {
        info.cpu_core_count = core_count;
    }

    // Total RAM
    std::uint64_t ram = 0;
    std::size_t ram_len = sizeof(ram);
    if (sysctlbyname("hw.memsize", &ram, &ram_len, nullptr, 0) == 0) {
        info.total_ram_bytes = ram;
    }

#elif defined(__linux__)
    // CPU info
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("model name") != std::string::npos) {
                auto colon = line.find(':');
                if (colon != std::string::npos) {
                    info.cpu_brand = line.substr(colon + 2);
                }
            }
            if (line.find("processor") != std::string::npos && line.find(':') != std::string::npos) {
                info.cpu_core_count++;
            }
        }
    }

    // Total RAM
    std::ifstream meminfo("/proc/meminfo");
    if (meminfo.is_open()) {
        std::string line;
        while (std::getline(meminfo, line)) {
            if (line.find("MemTotal:") != std::string::npos) {
                // Format: "MemTotal:       12345678 kB"
                std::stringstream ss(line);
                std::string label;
                std::uint64_t kb;
                ss >> label >> kb;
                info.total_ram_bytes = kb * 1024;
                break;
            }
        }
    }

    // OS version
    std::ifstream osrelease("/etc/os-release");
    if (osrelease.is_open()) {
        std::string line;
        while (std::getline(osrelease, line)) {
            if (line.find("PRETTY_NAME=") != std::string::npos) {
                auto eq = line.find('=');
                auto val = line.substr(eq + 1);
                // Strip quotes
                if (val.size() >= 2 && val[0] == '"' && val.back() == '"') {
                    val = val.substr(1, val.size() - 2);
                }
                info.os_name = val;
                break;
            }
        }
    }
    if (info.os_name.empty()) {
        info.os_name = "Linux (unknown distro)";
    }
#else
    info.os_name = "Unknown OS";
    info.cpu_brand = "Unknown CPU";
#endif

    return info;
}

// ===================================================================
// collect_log_tail
// ===================================================================

std::vector<std::string> collect_log_tail(const std::filesystem::path& log_path, int max_lines) {
    std::vector<std::string> lines;
    std::ifstream file(log_path);
    if (!file.is_open()) {
        lines.push_back("<log file not found: " + log_path.string() + ">");
        return lines;
    }

    // Read all lines into a ring buffer of max_lines
    std::vector<std::string> buffer(static_cast<std::size_t>(max_lines));
    std::size_t index = 0;
    std::size_t count = 0;

    std::string line;
    while (std::getline(file, line)) {
        buffer[index] = line;
        index = (index + 1) % static_cast<std::size_t>(max_lines);
        if (count < static_cast<std::size_t>(max_lines)) ++count;
    }

    // Reconstruct in order
    lines.reserve(count);
    std::size_t start = (count < static_cast<std::size_t>(max_lines)) ? 0 : index;
    for (std::size_t i = 0; i < count; ++i) {
        lines.push_back(buffer[(start + i) % static_cast<std::size_t>(max_lines)]);
    }

    return lines;
}

// ===================================================================
// collect_crash_summary
// ===================================================================

std::string collect_crash_summary(const std::filesystem::path& crash_dir) {
    auto dumps = list_crash_dumps(crash_dir);
    if (dumps.empty()) {
        return "No crash dumps found.\n";
    }

    std::string summary;
    summary += "Found " + std::to_string(dumps.size()) + " crash dump(s):\n";
    for (const auto& dump : dumps) {
        auto ctx = read_crash_dump(dump);
        summary += "  - " + dump.filename().string() + "\n";
        summary += "    Signal: " + ctx.signal_name + " (" + std::to_string(ctx.signal_num) + ")\n";
        summary += "    Time:   " + std::to_string(ctx.timestamp_sec) + "\n";
        summary += "    Frames: " + std::to_string(ctx.frames.size()) + "\n";
        if (!ctx.frames.empty()) {
            summary += "    Top:    " + ctx.frames[0].symbol + "\n";
        }
    }

    return summary;
}

// ===================================================================
// write_diagnostic_bundle
// ===================================================================

std::filesystem::path write_diagnostic_bundle(const std::filesystem::path& output_dir) {
    std::error_code ec;
    std::filesystem::create_directories(output_dir, ec);
    if (ec) {
        log_error_cat(AE_LOG_CATEGORY, "Failed to create diagnostics directory: " + output_dir.string());
        return {};
    }

    // Use a monotonic counter to avoid collisions when multiple bundles
    // are created in the same second.
    static std::atomic<int> bundle_counter{0};
    auto ts = static_cast<std::int64_t>(now_seconds());
    int seq = bundle_counter.fetch_add(1, std::memory_order_relaxed);
    std::string bundle_name = "diagnostic_" + std::to_string(ts) + "_" + std::to_string(seq);
    auto bundle_dir = output_dir / bundle_name;

    std::filesystem::create_directories(bundle_dir, ec);
    if (ec) {
        log_error_cat(AE_LOG_CATEGORY, "Failed to create bundle directory: " + bundle_dir.string());
        return {};
    }

    // --- System info ---
    {
        auto info = collect_system_info();
        std::ofstream file(bundle_dir / "system_info.txt");
        if (file.is_open()) {
            file << "=== System Information ===" << "\n";
            file << "OS:           " << info.os_name << "\n";
            file << "CPU:          " << info.cpu_brand << "\n";
            file << "Cores:        " << info.cpu_core_count << "\n";
            file << "Total RAM:    " << (info.total_ram_bytes / (1024 * 1024)) << " MB\n";
            file << "GPU:          " << info.gpu_renderer << "\n";
            file << "Engine Ver:   " << info.engine_version << "\n";
        }
    }

    // --- Config dump ---
    {
        std::ofstream file(bundle_dir / "config_dump.txt");
        if (file.is_open()) {
            file << "=== Config Variables ===" << "\n";
            auto& registry = ConfigRegistry::instance();
            auto keys = registry.all_keys();
            for (const auto& key : keys) {
                file << key << " = " << registry.get_value(key) << "\n";
            }
        }
    }

    // --- Log tail ---
    {
        auto log_lines = collect_log_tail();
        std::ofstream file(bundle_dir / "log_tail.txt");
        if (file.is_open()) {
            file << "=== Log Tail (last " << log_lines.size() << " lines) ===" << "\n";
            for (const auto& line : log_lines) {
                file << line << "\n";
            }
        }
    }

    // --- Crash summary ---
    {
        auto summary = collect_crash_summary();
        std::ofstream file(bundle_dir / "crash_summary.txt");
        if (file.is_open()) {
            file << "=== Crash Summary ===" << "\n";
            file << summary;
        }
    }

    log_info_cat(AE_LOG_CATEGORY, "Diagnostic bundle written to " + bundle_dir.string());
    return bundle_dir;
}

// ===================================================================
// list_diagnostic_bundles
// ===================================================================

std::vector<std::filesystem::path> list_diagnostic_bundles(const std::filesystem::path& output_dir) {
    std::vector<std::filesystem::path> bundles;
    std::error_code ec;

    if (!std::filesystem::exists(output_dir, ec)) return bundles;

    for (const auto& entry : std::filesystem::directory_iterator(output_dir, ec)) {
        if (entry.is_directory() &&
            entry.path().filename().string().find("diagnostic_") == 0) {
            bundles.push_back(entry.path());
        }
    }

    std::sort(bundles.begin(), bundles.end(),
              [](const std::filesystem::path& a, const std::filesystem::path& b) {
                  std::error_code ec_a, ec_b;
                  auto ta = std::filesystem::last_write_time(a, ec_a);
                  auto tb = std::filesystem::last_write_time(b, ec_b);
                  if (ec_a || ec_b) return false;
                  return ta > tb;
              });

    return bundles;
}

}  // namespace ae
