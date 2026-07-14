#include "ae/core/job_system.h"
#include "ae/core/log.h"

#include <algorithm>
#include <mutex>

#define AE_LOG_CATEGORY "Core"

namespace ae {

JobSystem::~JobSystem() {
    shutdown();
}

void JobSystem::init(int thread_count) {
    if (running_) {
        log_debug_cat(AE_LOG_CATEGORY, "JobSystem::init called but already running");
        return;
    }

    if (thread_count <= 0) {
        int hw = static_cast<int>(std::thread::hardware_concurrency());
        thread_count = std::max(1, hw - 1);  // leave one core for main thread
        if (thread_count < 1) thread_count = 1;
    }

    log_info_cat(AE_LOG_CATEGORY, "JobSystem initializing with " + std::to_string(thread_count) + " worker thread(s)");

    running_ = true;
    workers_.reserve(static_cast<std::size_t>(thread_count));
    for (int i = 0; i < thread_count; ++i) {
        workers_.push_back({std::thread(&JobSystem::worker_loop, this, i), i});
    }
}

void JobSystem::shutdown() {
    if (!running_) {
        log_debug_cat(AE_LOG_CATEGORY, "JobSystem::shutdown called but not running");
        return;
    }

    log_info_cat(AE_LOG_CATEGORY, "JobSystem shutting down (" + std::to_string(workers_.size()) + " worker thread(s))");
    {
        // The condition-variable predicate reads running_ while holding
        // jobs_mutex_. Mutate it under the same mutex so a worker cannot
        // miss the shutdown notification between checking and waiting.
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        running_ = false;
    }
    jobs_cv_.notify_all();

    for (auto& w : workers_) {
        if (w.thread.joinable()) w.thread.join();
    }
    workers_.clear();

    std::lock_guard<std::mutex> lock(jobs_mutex_);
    jobs_.clear();
    ready_jobs_.clear();

    log_info_cat(AE_LOG_CATEGORY, "JobSystem shutdown complete");
}

JobSystem::JobHandle JobSystem::submit(JobFunction fn) {
    std::lock_guard<std::mutex> lock(jobs_mutex_);
    int idx = static_cast<int>(jobs_.size());
    auto job = std::make_unique<Job>();
    job->function = std::move(fn);
    job->unfinished_parents.store(0, std::memory_order_release);
    job->completed = false;
    jobs_.push_back(std::move(job));
    ready_jobs_.push_back(idx);
    jobs_remaining_.fetch_add(1, std::memory_order_release);
    jobs_cv_.notify_one();
    return {idx};
}

JobSystem::JobHandle JobSystem::submit_after(JobHandle parent, JobFunction fn) {
    std::lock_guard<std::mutex> lock(jobs_mutex_);
    int idx = static_cast<int>(jobs_.size());
    auto job = std::make_unique<Job>();
    job->function = std::move(fn);
    job->completed = false;

    bool has_dependency = false;
    if (parent.valid() && parent.index < idx) {
        auto& parent_job = jobs_[static_cast<std::size_t>(parent.index)];
        if (!parent_job->completed) {
            job->unfinished_parents.store(1, std::memory_order_release);
            parent_job->children.push_back(idx);
            has_dependency = true;
        }
    }

    if (!has_dependency) {
        job->unfinished_parents.store(0, std::memory_order_release);
        ready_jobs_.push_back(idx);
        jobs_cv_.notify_one();
    }

    jobs_.push_back(std::move(job));
    jobs_remaining_.fetch_add(1, std::memory_order_release);

    return {idx};
}

JobSystem::JobHandle JobSystem::submit_after_all(const std::vector<JobHandle>& parents, JobFunction fn) {
    std::lock_guard<std::mutex> lock(jobs_mutex_);
    int idx = static_cast<int>(jobs_.size());
    auto job = std::make_unique<Job>();
    job->function = std::move(fn);
    job->completed = false;

    int pending = 0;
    for (const auto& parent : parents) {
        if (parent.valid() && parent.index < static_cast<int>(jobs_.size())) {
            auto& parent_job = jobs_[static_cast<std::size_t>(parent.index)];
            if (!parent_job->completed) {
                ++pending;
                parent_job->children.push_back(idx);
            }
        }
    }

    if (pending > 0) {
        job->unfinished_parents.store(pending, std::memory_order_release);
    } else {
        job->unfinished_parents.store(0, std::memory_order_release);
        ready_jobs_.push_back(idx);
        jobs_cv_.notify_all();
    }

    jobs_.push_back(std::move(job));
    jobs_remaining_.fetch_add(1, std::memory_order_release);

    return {idx};
}

std::vector<JobSystem::JobHandle> JobSystem::dispatch(int count, std::function<void(int)> fn) {
    std::vector<JobHandle> handles;
    handles.reserve(static_cast<std::size_t>(count));

    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        for (int i = 0; i < count; ++i) {
            int idx = static_cast<int>(jobs_.size());
            auto job = std::make_unique<Job>();
            job->function = [fn, i]() { fn(i); };
            job->unfinished_parents.store(0, std::memory_order_release);
            job->completed = false;
            jobs_.push_back(std::move(job));
            ready_jobs_.push_back(idx);
            handles.push_back({idx});
        }
        jobs_remaining_.fetch_add(count, std::memory_order_release);
        jobs_cv_.notify_all();
    }

    return handles;
}

int JobSystem::pop_ready_job_locked() {
    if (ready_jobs_.empty()) return -1;
    int job_idx = ready_jobs_.back();
    ready_jobs_.pop_back();
    return job_idx;
}

bool JobSystem::execute_job(int job_idx) {
    if (job_idx < 0) return false;

    JobFunction job_fn;
    std::vector<int> children;
    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        auto& job = jobs_[static_cast<std::size_t>(job_idx)];
        job_fn = std::move(job->function);
        job->function = nullptr;
    }

    if (job_fn) job_fn();

    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        auto& job = jobs_[static_cast<std::size_t>(job_idx)];
        job->completed = true;
        children = std::move(job->children);

        for (int child_idx : children) {
            auto& child_job = jobs_[static_cast<std::size_t>(child_idx)];
            int parents = child_job->unfinished_parents.fetch_sub(1, std::memory_order_acq_rel);
            if (parents == 1) {
                ready_jobs_.push_back(child_idx);
            }
        }

        // wait_all() observes this value in a condition-variable predicate.
        // Keep the state transition under jobs_mutex_ to prevent a lost wake.
        jobs_remaining_.fetch_sub(1, std::memory_order_release);
    }

    jobs_cv_.notify_all();

    return true;
}

void JobSystem::wait(JobHandle handle) {
    if (!handle.valid()) return;

    std::unique_lock<std::mutex> lock(jobs_mutex_);
    jobs_cv_.wait(lock, [&]() {
        return !running_.load(std::memory_order_acquire) ||
               (handle.index < static_cast<int>(jobs_.size()) &&
                jobs_[static_cast<std::size_t>(handle.index)]->completed);
    });
}

void JobSystem::wait_all() {
    std::unique_lock<std::mutex> lock(jobs_mutex_);
    jobs_cv_.wait(lock, [&]() {
        return !running_.load(std::memory_order_acquire) ||
               jobs_remaining_.load(std::memory_order_acquire) == 0;
    });
}

void JobSystem::worker_loop(int /*worker_index*/) {
    while (running_.load(std::memory_order_acquire)) {
        int job_to_run = -1;
        {
            std::unique_lock<std::mutex> lock(jobs_mutex_);
            jobs_cv_.wait(lock, [&]() {
                return !running_.load(std::memory_order_acquire) || !ready_jobs_.empty();
            });

            if (!running_.load(std::memory_order_acquire)) break;
            job_to_run = pop_ready_job_locked();
        }

        if (job_to_run >= 0) {
            execute_job(job_to_run);
        }
    }
}

}  // namespace ae
