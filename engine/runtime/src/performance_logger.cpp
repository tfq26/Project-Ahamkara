#include "ae/runtime/performance_logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace ae {

bool PerformanceLogger::open(const std::string& prefix) {
    close();

    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_local = std::localtime(&t);

    std::ostringstream filename;
    filename << prefix << "_"
             << std::put_time(tm_local, "%Y%m%d_%H%M%S")
             << ".csv";

    file_ = std::fopen(filename.str().c_str(), "w");
    if (file_ == nullptr) return false;

    // Write CSV header
    std::fprintf(file_,
        "frame,fps,frame_ms,p1_low_fps,p1_high_fps,"
        "cpu_proc_pct,cpu_sys_pct,rss_mb,vmem_mb,"
        "sys_used_mem_mb,sys_total_mem_mb,gpu_pct\n");
    std::fflush(file_);

    return true;
}

void PerformanceLogger::close() {
    if (file_ != nullptr) {
        std::fclose(file_);
        file_ = nullptr;
    }
}

void PerformanceLogger::log(uint64_t frame_number,
                            const RuntimeMetricsSnapshot& metrics) {
    if (file_ == nullptr) return;

    std::fprintf(file_,
        "%llu,%.1f,%.2f,%.1f,%.1f,"
        "%.2f,%.2f,%.2f,%.2f,"
        "%.2f,%.2f,%.2f\n",
        static_cast<unsigned long long>(frame_number),
        metrics.fps,
        metrics.frame_time_ms,
        metrics.fps_p1_low,
        metrics.fps_p1_high,
        metrics.process_cpu_percent,
        metrics.system_cpu_percent,
        metrics.process_rss_mb,
        metrics.process_virtual_mb,
        metrics.system_used_memory_mb,
        metrics.system_total_memory_mb,
        metrics.gpu_usage_available ? metrics.gpu_usage_percent : -1.0);

    // Flush after every write so we don't lose data on crash.
    std::fflush(file_);
}

}  // namespace ae
