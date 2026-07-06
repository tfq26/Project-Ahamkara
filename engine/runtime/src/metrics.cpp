#include "ae/runtime/metrics.h"
#include "ae/core/log.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#define AE_LOG_CATEGORY "Runtime"

#if !defined(_WIN32)
#include <sys/resource.h>
#endif

#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <sys/sysctl.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#endif

namespace ae {
namespace {

constexpr double kBytesPerMegabyte = 1024.0 * 1024.0;

double current_wall_seconds() {
    using clock = std::chrono::steady_clock;
    static const auto start_time = clock::now();
    const auto elapsed = clock::now() - start_time;
    return std::chrono::duration<double>(elapsed).count();
}

#if !defined(_WIN32)
double process_cpu_seconds() {
    rusage usage {};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        log_warning_cat(AE_LOG_CATEGORY, "getrusage failed for process CPU metric");
        return 0.0;
    }

    const double user_seconds =
        static_cast<double>(usage.ru_utime.tv_sec) + static_cast<double>(usage.ru_utime.tv_usec) / 1'000'000.0;
    const double system_seconds =
        static_cast<double>(usage.ru_stime.tv_sec) + static_cast<double>(usage.ru_stime.tv_usec) / 1'000'000.0;
    return user_seconds + system_seconds;
}
#elif defined(_WIN32)
double process_cpu_seconds() {
    FILETIME create, exit, kernel, user;
    if (GetProcessTimes(GetCurrentProcess(), &create, &exit, &kernel, &user)) {
        const ULARGE_INTEGER k = {{kernel.dwLowDateTime, kernel.dwHighDateTime}};
        const ULARGE_INTEGER u = {{user.dwLowDateTime, user.dwHighDateTime}};
        return static_cast<double>(k.QuadPart + u.QuadPart) / 10000000.0; // 100ns units → seconds
    }
    return 0.0;
}
#endif

#if defined(__APPLE__)
bool read_macos_system_cpu_ticks(double& total_ticks, double& active_ticks) {
    host_cpu_load_info_data_t cpu_info {};
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, reinterpret_cast<host_info_t>(&cpu_info), &count)
        != KERN_SUCCESS) {
        log_warning_cat(AE_LOG_CATEGORY, "host_statistics failed for system CPU metric");
        return false;
    }

    const double user = static_cast<double>(cpu_info.cpu_ticks[CPU_STATE_USER]);
    const double system = static_cast<double>(cpu_info.cpu_ticks[CPU_STATE_SYSTEM]);
    const double idle = static_cast<double>(cpu_info.cpu_ticks[CPU_STATE_IDLE]);
    const double nice = static_cast<double>(cpu_info.cpu_ticks[CPU_STATE_NICE]);

    total_ticks = user + system + idle + nice;
    active_ticks = user + system + nice;
    return true;
}

RuntimeMetricsSnapshot read_macos_memory_metrics() {
    RuntimeMetricsSnapshot snapshot {};

    mach_task_basic_info_data_t task_basic_info {};
    mach_msg_type_number_t task_info_count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(),
                  MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&task_basic_info),
                  &task_info_count)
        == KERN_SUCCESS) {
        snapshot.process_rss_mb = static_cast<double>(task_basic_info.resident_size) / kBytesPerMegabyte;
        snapshot.process_virtual_mb = static_cast<double>(task_basic_info.virtual_size) / kBytesPerMegabyte;
    } else {
        log_warning_cat(AE_LOG_CATEGORY, "task_info failed for process memory metric");
    }

    vm_size_t page_size = 0;
    host_page_size(mach_host_self(), &page_size);

    vm_statistics64_data_t vm_stats {};
    mach_msg_type_number_t vm_count = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vm_stats), &vm_count)
        == KERN_SUCCESS) {
        const double used_pages = static_cast<double>(
            vm_stats.active_count + vm_stats.inactive_count + vm_stats.wire_count + vm_stats.compressor_page_count);
        snapshot.system_used_memory_mb = used_pages * static_cast<double>(page_size) / kBytesPerMegabyte;
    } else {
        log_warning_cat(AE_LOG_CATEGORY, "host_statistics64 failed for system memory metric");
    }

    std::uint64_t total_memory_bytes = 0;
    size_t length = sizeof(total_memory_bytes);
    if (sysctlbyname("hw.memsize", &total_memory_bytes, &length, nullptr, 0) == 0) {
        snapshot.system_total_memory_mb = static_cast<double>(total_memory_bytes) / kBytesPerMegabyte;
    }

    return snapshot;
}
#elif defined(__linux__)
bool read_linux_system_cpu_ticks(double& total_ticks, double& active_ticks) {
    std::ifstream stat_file("/proc/stat");
    if (!stat_file.is_open()) {
        log_warning_cat(AE_LOG_CATEGORY, "Cannot open /proc/stat for system CPU metric");
        return false;
    }

    std::string cpu_label;
    double user = 0.0;
    double nice = 0.0;
    double system = 0.0;
    double idle = 0.0;
    double iowait = 0.0;
    double irq = 0.0;
    double softirq = 0.0;
    double steal = 0.0;
    stat_file >> cpu_label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
    if (cpu_label != "cpu") {
        return false;
    }

    total_ticks = user + nice + system + idle + iowait + irq + softirq + steal;
    active_ticks = user + nice + system + irq + softirq + steal;
    return true;
}

RuntimeMetricsSnapshot read_linux_memory_metrics() {
    RuntimeMetricsSnapshot snapshot {};

    long page_size = sysconf(_SC_PAGESIZE);
    std::ifstream statm_file("/proc/self/statm");
    if (statm_file.is_open()) {
        double virtual_pages = 0.0;
        double resident_pages = 0.0;
        statm_file >> virtual_pages >> resident_pages;
        snapshot.process_virtual_mb = virtual_pages * static_cast<double>(page_size) / kBytesPerMegabyte;
        snapshot.process_rss_mb = resident_pages * static_cast<double>(page_size) / kBytesPerMegabyte;
    }

    std::ifstream meminfo_file("/proc/meminfo");
    if (meminfo_file.is_open()) {
        std::string key;
        double value_kb = 0.0;
        std::string unit;
        double total_kb = 0.0;
        double available_kb = 0.0;

        while (meminfo_file >> key >> value_kb >> unit) {
            if (key == "MemTotal:") {
                total_kb = value_kb;
            } else if (key == "MemAvailable:") {
                available_kb = value_kb;
            }
        }

        snapshot.system_total_memory_mb = total_kb / 1024.0;
        snapshot.system_used_memory_mb = std::max(0.0, (total_kb - available_kb) / 1024.0);
    }

    return snapshot;
}
#elif defined(_WIN32)
bool read_windows_system_cpu_ticks(double& total_ticks, double& active_ticks) {
    FILETIME idle, kernel, user;
    if (GetSystemTimes(&idle, &kernel, &user)) {
        const ULARGE_INTEGER idle_time = {{idle.dwLowDateTime, idle.dwHighDateTime}};
        const ULARGE_INTEGER kernel_time = {{kernel.dwLowDateTime, kernel.dwHighDateTime}};
        const ULARGE_INTEGER user_time = {{user.dwLowDateTime, user.dwHighDateTime}};

        // Kernel time includes idle time on Windows
        const double idle_val = static_cast<double>(idle_time.QuadPart);
        const double kernel_val = static_cast<double>(kernel_time.QuadPart);
        const double user_val = static_cast<double>(user_time.QuadPart);

        total_ticks = kernel_val + user_val;
        active_ticks = kernel_val - idle_val + user_val;
        return true;
    }
    log_warning_cat(AE_LOG_CATEGORY, "GetSystemTimes failed for system CPU metric");
    return false;
}

RuntimeMetricsSnapshot read_windows_memory_metrics() {
    RuntimeMetricsSnapshot snapshot;
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        snapshot.process_rss_mb = static_cast<double>(pmc.WorkingSetSize) / kBytesPerMegabyte;
        snapshot.process_virtual_mb = static_cast<double>(pmc.PagefileUsage) / kBytesPerMegabyte;
    }
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        snapshot.system_total_memory_mb = static_cast<double>(ms.ullTotalPhys) / kBytesPerMegabyte;
        snapshot.system_used_memory_mb = static_cast<double>(ms.ullTotalPhys - ms.ullAvailPhys) / kBytesPerMegabyte;
    }
    return snapshot;
}
#endif

}  // namespace

RuntimeMetricsCollector::RuntimeMetricsCollector() {
    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    logical_core_count_ = hardware_threads > 0 ? static_cast<double>(hardware_threads) : 1.0;
    log_debug_cat(AE_LOG_CATEGORY, "RuntimeMetricsCollector initialized: logical_cores=" + std::to_string(logical_core_count_));
}

RuntimeMetricsCollector::CpuTimes RuntimeMetricsCollector::read_cpu_times() const {
    CpuTimes cpu_times {};
    cpu_times.process_seconds = process_cpu_seconds();
    cpu_times.wall_seconds = current_wall_seconds();

#if defined(__APPLE__)
    read_macos_system_cpu_ticks(cpu_times.system_total_ticks, cpu_times.system_active_ticks);
#elif defined(__linux__)
    read_linux_system_cpu_ticks(cpu_times.system_total_ticks, cpu_times.system_active_ticks);
#elif defined(_WIN32)
    read_windows_system_cpu_ticks(cpu_times.system_total_ticks, cpu_times.system_active_ticks);
#endif

    return cpu_times;
}

RuntimeMetricsSnapshot RuntimeMetricsCollector::read_memory_metrics() const {
#if defined(__APPLE__)
    return read_macos_memory_metrics();
#elif defined(__linux__)
    return read_linux_memory_metrics();
#elif defined(_WIN32)
    return read_windows_memory_metrics();
#else
    return {};
#endif
}

RuntimeMetricsSnapshot RuntimeMetricsCollector::sample(double frame_time_seconds, bool compute_percentiles) {
    RuntimeMetricsSnapshot snapshot = read_memory_metrics();
    snapshot.frame_time_ms = frame_time_seconds * 1000.0;
    snapshot.fps = frame_time_seconds > 0.0 ? 1.0 / frame_time_seconds : 0.0;
    snapshot.gpu_usage_available = false;
    snapshot.gpu_usage_percent = 0.0;

    // Track frame time history for percentile computation
    frame_time_history_[history_index_] = frame_time_seconds;
    history_index_ = (history_index_ + 1) % kHistorySize;
    if (history_count_ < kHistorySize) ++history_count_;

    if (compute_percentiles && history_count_ >= 100) {
        // Copy and sort the active portion of the ring buffer
        std::array<double, kHistorySize> sorted;
        for (std::size_t i = 0; i < history_count_; ++i) {
            const std::size_t src = (history_index_ + kHistorySize - history_count_ + i) % kHistorySize;
            sorted[i] = frame_time_history_[src];
        }
        std::sort(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(history_count_));

        // 1% low: the frame time at the 99th percentile (worst 1%)
        const std::size_t idx_low = static_cast<std::size_t>(static_cast<double>(history_count_) * 0.99);
        const std::size_t idx_high = static_cast<std::size_t>(static_cast<double>(history_count_) * 0.01);
        const std::size_t idx_clamp = history_count_ > 0 ? history_count_ - 1 : 0;

        const double ft_low = sorted[std::min(idx_low, idx_clamp)];
        const double ft_high = sorted[std::min(idx_high, idx_clamp)];
        snapshot.fps_p1_low = ft_low > 0.0 ? 1.0 / ft_low : 0.0;
        snapshot.fps_p1_high = ft_high > 0.0 ? 1.0 / ft_high : 0.0;
    }

    const CpuTimes current_cpu_times = read_cpu_times();
    if (has_previous_sample_) {
        const double process_delta = current_cpu_times.process_seconds - previous_cpu_times_.process_seconds;
        const double wall_delta = current_cpu_times.wall_seconds - previous_cpu_times_.wall_seconds;
        if (wall_delta > 0.0) {
            snapshot.process_cpu_percent =
                std::clamp((process_delta / (wall_delta * logical_core_count_)) * 100.0, 0.0, 100.0);
        }

        const double system_total_delta = current_cpu_times.system_total_ticks - previous_cpu_times_.system_total_ticks;
        const double system_active_delta =
            current_cpu_times.system_active_ticks - previous_cpu_times_.system_active_ticks;
        if (system_total_delta > 0.0) {
            snapshot.system_cpu_percent =
                std::clamp((system_active_delta / system_total_delta) * 100.0, 0.0, 100.0);
        }
    }

    previous_cpu_times_ = current_cpu_times;
    has_previous_sample_ = true;
    return snapshot;
}

}  // namespace ae
