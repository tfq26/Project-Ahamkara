#include "ae/core/job_system.h"

#include <algorithm>
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

    std::lock_guard<std::mutex> lock(jobs_mutex_);
    jobs_.clear();
    ready_jobs_.clear();
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
    }

    jobs_.push_back(std::move(job));
    jobs_remaining_.fetch_add(1, std::memory_order_release);

    return {idx};
}

void JobSystem::wait(JobHandle handle) {
    if (!handle.valid()) return;

    // Spin-wait until the specific job is completed
    while (true) {
        {
            std::lock_guard<std::mutex> lock(jobs_mutex_);
            if (handle.index < static_cast<int>(jobs_.size()) && jobs_[static_cast<std::size_t>(handle.index)]->completed) {
                break;
            }
        }

        // Try to help process ready jobs on the main thread
        int job_to_run = -1;
        {
            std::lock_guard<std::mutex> lock(jobs_mutex_);
            if (!ready_jobs_.empty()) {
                job_to_run = ready_jobs_.back();
                ready_jobs_.pop_back();
            }
        }

        if (job_to_run >= 0) {
            JobFunction job_fn;
            std::vector<int> children;
            {
                std::lock_guard<std::mutex> lock(jobs_mutex_);
                auto& job = jobs_[static_cast<std::size_t>(job_to_run)];
                job_fn = std::move(job->function);
                job->function = nullptr;
                children = job->children;
            }

            if (job_fn) job_fn();

            {
                std::lock_guard<std::mutex> lock(jobs_mutex_);
                auto& job = jobs_[static_cast<std::size_t>(job_to_run)];
                job->completed = true;

                for (int child_idx : children) {
                    auto& child_job = jobs_[static_cast<std::size_t>(child_idx)];
                    int parents = child_job->unfinished_parents.fetch_sub(1, std::memory_order_acq_rel);
                    if (parents == 1) { // was 1, now 0
                        ready_jobs_.push_back(child_idx);
                    }
                }
            }
            jobs_remaining_.fetch_sub(1, std::memory_order_release);
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    }
}

void JobSystem::wait_all() {
    while (jobs_remaining_.load(std::memory_order_acquire) > 0) {
        int job_to_run = -1;
        {
            std::lock_guard<std::mutex> lock(jobs_mutex_);
            if (!ready_jobs_.empty()) {
                job_to_run = ready_jobs_.back();
                ready_jobs_.pop_back();
            }
        }

        if (job_to_run >= 0) {
            JobFunction job_fn;
            std::vector<int> children;
            {
                std::lock_guard<std::mutex> lock(jobs_mutex_);
                auto& job = jobs_[static_cast<std::size_t>(job_to_run)];
                job_fn = std::move(job->function);
                job->function = nullptr;
                children = job->children;
            }

            if (job_fn) job_fn();

            {
                std::lock_guard<std::mutex> lock(jobs_mutex_);
                auto& job = jobs_[static_cast<std::size_t>(job_to_run)];
                job->completed = true;

                for (int child_idx : children) {
                    auto& child_job = jobs_[static_cast<std::size_t>(child_idx)];
                    int parents = child_job->unfinished_parents.fetch_sub(1, std::memory_order_acq_rel);
                    if (parents == 1) { // was 1, now 0
                        ready_jobs_.push_back(child_idx);
                    }
                }
            }
            jobs_remaining_.fetch_sub(1, std::memory_order_release);
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    }
}

void JobSystem::worker_loop(int worker_index) {
    (void)worker_index;
    while (running_.load(std::memory_order_acquire)) {
        int job_to_run = -1;
        {
            std::lock_guard<std::mutex> lock(jobs_mutex_);
            if (!ready_jobs_.empty()) {
                job_to_run = ready_jobs_.back();
                ready_jobs_.pop_back();
            }
        }

        if (job_to_run >= 0) {
            JobFunction job_fn;
            std::vector<int> children;
            {
                std::lock_guard<std::mutex> lock(jobs_mutex_);
                auto& job = jobs_[static_cast<std::size_t>(job_to_run)];
                job_fn = std::move(job->function);
                job->function = nullptr;
                children = job->children;
            }

            if (job_fn) job_fn();

            {
                std::lock_guard<std::mutex> lock(jobs_mutex_);
                auto& job = jobs_[static_cast<std::size_t>(job_to_run)];
                job->completed = true;

                for (int child_idx : children) {
                    auto& child_job = jobs_[static_cast<std::size_t>(child_idx)];
                    int parents = child_job->unfinished_parents.fetch_sub(1, std::memory_order_acq_rel);
                    if (parents == 1) { // was 1, now 0
                        ready_jobs_.push_back(child_idx);
                    }
                }
            }
            jobs_remaining_.fetch_sub(1, std::memory_order_release);
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    }
}

}  // namespace ae
