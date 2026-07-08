#include "ae/core/memory_budget.h"
#include "ae/core/frame_allocator.h"

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/task_info.h>
#else
// Linux / procfs
#include <fstream>
#include <string>
#endif

namespace ae {

MemoryBudgetTracker::MemoryBudgetTracker(
    std::size_t rss_soft_bytes,
    std::size_t rss_hard_bytes,
    std::size_t alloc_soft_bytes,
    std::size_t alloc_hard_bytes)
    : rss_soft_bytes_(rss_soft_bytes)
    , rss_hard_bytes_(rss_hard_bytes)
    , alloc_soft_bytes_(alloc_soft_bytes)
    , alloc_hard_bytes_(alloc_hard_bytes) {}

void MemoryBudgetTracker::update_rss() {
#if defined(__APPLE__)
    task_vm_info_data_t info;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    kern_return_t kr = task_info(mach_task_self(), TASK_VM_INFO,
                                 reinterpret_cast<task_info_t>(&info), &count);
    if (kr == KERN_SUCCESS) {
        // resident_size is in bytes
        rss_bytes_ = static_cast<std::size_t>(info.resident_size);
    }
#elif defined(_WIN32) || defined(_WIN64)
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        rss_bytes_ = static_cast<std::size_t>(pmc.WorkingSetSize);
    }
#else
    // Linux: read /proc/self/status
    std::ifstream proc("/proc/self/status");
    if (proc.is_open()) {
        std::string line;
        while (std::getline(proc, line)) {
            if (line.compare(0, 6, "VmRSS:") == 0) {
                // Format: "VmRSS:   12345 kB"
                std::size_t value = 0;
                auto it = line.begin() + 6;
                // skip spaces
                while (it != line.end() && (*it == ' ' || *it == '\t')) ++it;
                std::string num_str;
                while (it != line.end() && *it >= '0' && *it <= '9') {
                    num_str.push_back(*it);
                    ++it;
                }
                if (!num_str.empty()) {
                    value = static_cast<std::size_t>(std::stoul(num_str)) * 1024; // kB → bytes
                }
                rss_bytes_ = value;
                break;
            }
        }
    }
#endif

    if (rss_bytes_ > peak_rss_bytes_) {
        peak_rss_bytes_ = rss_bytes_;
    }

    rss_pressure_ = compute_pressure(rss_bytes_, rss_soft_bytes_, rss_hard_bytes_);
}

void MemoryBudgetTracker::track_frame_allocator(const FrameAllocator& alloc) {
    alloc_peak_bytes_ = alloc.peak_used();
    alloc_pressure_ = compute_pressure(alloc_peak_bytes_,
                                       alloc_soft_bytes_,
                                       alloc_hard_bytes_);
}

auto MemoryBudgetTracker::compute_pressure(std::size_t value,
                                            std::size_t soft,
                                            std::size_t hard) const -> Pressure {
    if (hard > 0 && value >= hard) return Pressure::Critical;
    if (soft > 0 && value >= soft) return Pressure::Warning;
    return Pressure::Ok;
}

void MemoryBudgetTracker::reset() {
    rss_bytes_ = 0;
    peak_rss_bytes_ = 0;
    alloc_peak_bytes_ = 0;
    rss_pressure_ = Pressure::Ok;
    alloc_pressure_ = Pressure::Ok;
}

}  // namespace ae
