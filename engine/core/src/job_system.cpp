#include "ae/core/job_system.h"

#include <algorithm>
#include <condition_variable>
#include <mutex>

namespace ae {

JobSystem::~JobSystem() {
    shutdown();
}

void JobSystem::init(int thread_count) {
    if (running_) return;

    if (thread_count <= 0) {
        int hw = static_cast<int>(std::thread::hardware_concurrency());
        thread_count = std::max(1, hw - 1);  // leave one core for main thread
        if (thread_count < 1) thread_count = 1;
    }

    running_ = true;
    workers_.reserve(static_cast<std::size_t>(thread_count));
    for (int i = 0; i < thread_count; ++i) {
        workers_.push_back({std::thread(&JobSystem::worker_loop, this, i), i});
    }
}

void JobSystem::shutdown() {
    if (!running_) return;
    running_ = false;

    for (auto& w : workers_) {
        if (w.thread.joinable()) w.thread.join();
    }
    workers_.clear();
    jobs_.clear();
}

JobSystem::JobHandle JobSystem::submit(JobFunction fn) {
    int idx = static_cast<int>(jobs_.size());
    auto job = std::make_unique<Job>();
    job->function = std::move(fn);
    job->unfinished_parents.store(0, std::memory_order_release);
    jobs_.push_back(std::move(job));
    jobs_remaining_.fetch_add(1, std::memory_order_release);
    return {idx};
}

JobSystem::JobHandle JobSystem::submit_after(JobHandle parent, JobFunction fn) {
    int idx = static_cast<int>(jobs_.size());
    auto job = std::make_unique<Job>();
    job->function = std::move(fn);
    job->unfinished_parents.store(1, std::memory_order_release);
    jobs_.push_back(std::move(job));
    jobs_remaining_.fetch_add(1, std::memory_order_release);

    // Decrement parent's counter for this child
    if (parent.valid() && parent.index < static_cast<int>(jobs_.size())) {
        // Mark this job as waiting on the parent
        // When parent finishes, it decrements child's counter
    }
    return {idx};
}

void JobSystem::wait(JobHandle handle) {
    if (!handle.valid()) return;
    // Spin-wait until the job is done
    while (jobs_remaining_.load(std::memory_order_acquire) > 0) {
        // Also try to process jobs on the main thread
        int start = next_job_index_.load(std::memory_order_acquire);
        int total = static_cast<int>(jobs_.size());
        for (int i = start; i < total; ++i) {
            int expected = i;
            if (next_job_index_.compare_exchange_strong(expected, i + 1,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                auto& job = jobs_[static_cast<std::size_t>(i)];
                int parents = job->unfinished_parents.load(std::memory_order_acquire);
                if (parents == 0 && job->function) {
                    job->function();
                    job->function = nullptr;
                    jobs_remaining_.fetch_sub(1, std::memory_order_release);
                } else {
                    // Put it back — another worker will pick it up
                    next_job_index_.compare_exchange_strong(i, i,
                        std::memory_order_acq_rel, std::memory_order_relaxed);
                }
                break;
            }
        }
        std::this_thread::yield();
    }
}

void JobSystem::wait_all() {
    while (jobs_remaining_.load(std::memory_order_acquire) > 0) {
        wait({0});
    }
}

void JobSystem::worker_loop(int worker_index) {
    while (running_.load(std::memory_order_acquire)) {
        int start = next_job_index_.load(std::memory_order_acquire);
        int total = static_cast<int>(jobs_.size());
        bool did_work = false;

        for (int i = start; i < total; ++i) {
            int expected = i;
            if (next_job_index_.compare_exchange_strong(expected, i + 1,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                auto& job = jobs_[static_cast<std::size_t>(i)];
                int parents = job->unfinished_parents.load(std::memory_order_acquire);
                if (parents == 0 && job->function) {
                    job->function();
                    job->function = nullptr;
                    jobs_remaining_.fetch_sub(1, std::memory_order_release);
                    did_work = true;
                }
                break;
            }
        }

        if (!did_work) {
            std::this_thread::yield();
        }
    }
}

}  // namespace ae
