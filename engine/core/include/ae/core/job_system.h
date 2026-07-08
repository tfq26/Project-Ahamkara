#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <mutex>

namespace ae {

/**
 * @brief Lightweight work-stealing job system.
 *
 * Creates N-1 worker threads (leaving one core for the main thread).
 * Jobs are submitted as void(void) callables. Dependencies are tracked
 * via atomic counters — a job only executes when all its parents complete.
 *
 * Usage:
 * @code
 *   JobSystem jobs;
 *   jobs.init();  // auto-detects thread count
 *
 *   auto handle = jobs.submit([]() {
 *       // do work...
 *   });
 *   jobs.wait(handle);
 *
 *   // Bulk dispatch
 *   std::vector<JobHandle> batch = jobs.dispatch(8, [](int i) {
 *       // parallel for body
 *   });
 *
 *   jobs.shutdown();
 * @endcode
 */
class JobSystem {
public:
    using JobFunction = std::function<void()>;

    struct JobHandle {
        int index {-1};
        bool valid() const { return index >= 0; }
    };

    JobSystem() = default;
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    /**
     * @brief Start worker threads.
     * @param thread_count Number of worker threads. 0 = auto (hardware_concurrency - 1, min 1).
     */
    void init(int thread_count = 0);

    /** Shut down workers. Blocks until all workers exit. */
    void shutdown();

    /**
     * @brief Submit a job for execution.
     * @param fn  The work to execute.
     * @return Handle that can be waited on.
     */
    [[nodiscard]] JobHandle submit(JobFunction fn);

    /**
     * @brief Submit a job with a single parent dependency.
     * The job only runs after the parent completes.
     */
    [[nodiscard]] JobHandle submit_after(JobHandle parent, JobFunction fn);

    /**
     * @brief Submit a job that waits for multiple parents to complete.
     * The job only runs after all parents in the list finish.
     */
    [[nodiscard]] JobHandle submit_after_all(const std::vector<JobHandle>& parents, JobFunction fn);

    /**
     * @brief Dispatch count jobs, each receiving its index [0, count).
     * @return Vector of count handles, one per job.
     */
    [[nodiscard]] std::vector<JobHandle> dispatch(int count, std::function<void(int)> fn);

    /** Block until a specific job completes. */
    void wait(JobHandle handle);

    /** Block until all pending jobs complete. */
    void wait_all();

    /** Number of worker threads. */
    [[nodiscard]] int worker_count() const { return static_cast<int>(workers_.size()); }

private:
    struct Job {
        JobFunction function;
        std::atomic<int> unfinished_parents {0};
        std::vector<int> children;
        bool completed {false};
    };

    /** Pop one ready job (under lock). Returns -1 if none. */
    int pop_ready_job_locked();

    /** Execute a job and notify dependents. Returns true if a job was run. */
    bool execute_job(int job_idx);

    void worker_loop(int worker_index);

    std::vector<std::unique_ptr<Job>> jobs_;
    std::vector<int> ready_jobs_;
    std::mutex jobs_mutex_;
    std::condition_variable jobs_cv_;
    std::atomic<int> jobs_remaining_ {0};
    std::atomic<bool> running_ {false};

    struct Worker {
        std::thread thread;
        int index;
    };
    std::vector<Worker> workers_;
};

}  // namespace ae
