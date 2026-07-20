#pragma once

// Streaming executor: product‑neutral async load/unload manager around
// residency transitions.
//
// Sits between a transition source (e.g. ResidencyManager) and the platform
// I/O layer.  Loader, storage, and scheduling are injected as pure virtual
// interfaces so that tests can substitute fake implementations and the
// executor never hard‑codes threading.
//
// State machine per region:
//   None → Requested → Loading → [Resident | Failed | Cancelled] → Evicting → None
//
// Budgets (memory and in‑flight) are enforced with deterministic ordering:
// requests are processed in FIFO order and budgets are checked before
// dispatching.
//
// Simulation‑visible residency changes happen ONLY at commit(), never
// during update().  This guarantees a consistent world view between
// sync points.

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "ae/core/residency_manager.h"

namespace ae::core {

// ---------------------------------------------------------------------------
// Injected interfaces
// ---------------------------------------------------------------------------

/// Interface for asynchronous loading of region data.
class IStreamingLoader {
public:
    /// Signature invoked when an async load completes.
    /// @param region_id  The region that was loading.
    /// @param success    true if the load succeeded.
    /// @param data       Pointer to loaded data (valid only until commit()).
    /// @param size       Size of loaded data in bytes.
    using Callback = std::function<void(int region_id, bool success,
                                        const void* data, std::size_t size)>;

    virtual ~IStreamingLoader() = default;

    /// Begin loading a region.  The loader must eventually invoke `callback`
    /// exactly once, from any thread, when the load completes or fails.
    /// The loader must tolerate multiple start_load() calls for the same
    /// region_id (retry).
    virtual void start_load(int region_id, Callback callback) = 0;

    /// Cancel any in‑flight load for this region.  After cancel_load()
    /// returns, the loader must NOT invoke the callback for this region.
    /// Safe to call when no load is in-flight (no‑op).
    virtual void cancel_load(int region_id) = 0;

    /// Human‑readable description of the most recent error.
    [[nodiscard]] virtual std::string last_error() const = 0;
};

/// Interface for storage and eviction of resident data.
class IStreamingStorage {
public:
    virtual ~IStreamingStorage() = default;

    /// Store loaded data under the given region id.  Returns false if
    /// the data cannot be stored (e.g. memory budget would be exceeded).
    virtual bool store(int region_id, const void* data, std::size_t size) = 0;

    /// Evict (unload) a region's stored data.
    virtual void evict(int region_id) = 0;

    /// Total bytes currently stored.
    [[nodiscard]] virtual std::size_t stored_bytes() const = 0;

    /// Maximum storage capacity in bytes (soft limit).
    [[nodiscard]] virtual std::size_t capacity_bytes() const = 0;
};

/// Scheduling interface that wraps ae::JobSystem (or a test stub).
class IStreamingScheduler {
public:
    virtual ~IStreamingScheduler() = default;

    /// Schedule a job for asynchronous execution on a worker thread.
    virtual void schedule(std::function<void()> job) = 0;
};

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------

/// Per‑region load state.
enum class RegionLoadState : std::uint8_t {
    None = 0,       ///< Not requested or fully evicted.
    Requested,      ///< Load requested, waiting for budget availability.
    Loading,        ///< Async load dispatched and in‑flight.
    Resident,       ///< Loaded and committed to simulation.
    Failed,         ///< Load failed after exhausting retries.
    Cancelled,      ///< Load cancelled before becoming resident.
    Evicting,       ///< Eviction dispatched.
};

/// A diagnostic record produced on failure or cancellation.
struct StreamingDiagnostic {
    int region_id{0};
    RegionLoadState state{RegionLoadState::None};
    std::string message;
};

/// A raw load result produced by the loader callback.
struct LoadResult {
    int region_id{0};
    bool success{false};
    const void* data{nullptr};
    std::size_t size{0};
};

// ---------------------------------------------------------------------------
// StreamingExecutor
// ---------------------------------------------------------------------------

/// Manages the async lifecycle of region loads and unloads.
///
/// Usage:
///   1. Construct with injected loader, storage, and scheduler.
///   2. Call request_load() / request_unload() when ResidencyManager
///      produces transitions.
///   3. Call update() each frame to advance state and dispatch work.
///   4. Call commit() at an explicit sync point to make completed
///      loads visible to the simulation.
///   5. Query state with state(), in_flight_count(), etc.
///
/// Thread safety: update() and commit() must be called from the main
/// (single) thread that owns the executor.  The loader's callback may
/// fire on any thread; it enqueues results via a mutex‑protected queue
/// that is drained during update().
class StreamingExecutor {
public:
    struct Budget {
        std::size_t max_storage_bytes{256U * 1024U * 1024U};  // 256 MB
        int max_in_flight{4};    ///< Max concurrent async loads.
        int max_retries{2};      ///< Retry attempts before marking Failed.
    };

    /// All three injected interfaces are required and must remain alive
    /// for the lifetime of the executor.
    StreamingExecutor(IStreamingLoader* loader,
                      IStreamingStorage* storage,
                      IStreamingScheduler* scheduler);

    // Non‑copyable.
    StreamingExecutor(const StreamingExecutor&) = delete;
    StreamingExecutor& operator=(const StreamingExecutor&) = delete;

    // -- Configuration --

    void set_budget(const Budget& budget) { budget_ = budget; }
    [[nodiscard]] const Budget& budget() const { return budget_; }

    // -- Lifecycle --

    /// Request loading of a region.  Idempotent if already requested,
    /// loading, or resident (no‑op).  A cancelled or failed region
    /// may be re‑requested.
    void request_load(int region_id, RegionCoord coord = {});

    /// Request unloading/eviction of a region.  Cancels any in‑flight
    /// load and schedules eviction if resident.
    void request_unload(int region_id);

    /// Advance the state machine: dispatch new loads within budget,
    /// handle completions, schedule evictions.  Must be called from
    /// the main thread once per tick.
    void update();

    /// Make all completed loads visible to the simulation.  After this
    /// call, completed regions transition to Resident and storage is
    /// committed.
    ///
    /// Call exactly once per frame, after all systems have had a chance
    /// to query state from the previous frame.
    void commit();

    /// Cancel a specific load.  No‑op if the region is already resident,
    /// evicting, or None.
    void cancel(int region_id);

    /// Cancel all pending and in‑flight loads.
    void cancel_all();

    // -- Queries --

    /// Current state of a region.
    [[nodiscard]] RegionLoadState state(int region_id) const;

    /// Number of loads currently in flight.
    [[nodiscard]] int in_flight_count() const { return in_flight_count_; }

    /// Number of pending (not yet dispatched) loads.
    [[nodiscard]] int pending_count() const { return static_cast<int>(pending_.size()); }

    /// Total stored bytes (delegated to storage interface).
    [[nodiscard]] std::size_t storage_used_bytes() const {
        return storage_ ? storage_->stored_bytes() : 0;
    }

    /// Number of regions in the Failed state.
    [[nodiscard]] int failed_count() const;

    /// Number of regions in the Cancelled state.
    [[nodiscard]] int cancelled_count() const;

    /// Number of regions in the Resident state.
    [[nodiscard]] int resident_count() const;

    /// Drain diagnostics accumulated since last call.
    [[nodiscard]] std::vector<StreamingDiagnostic> consume_diagnostics();

    /// Drain completed results (for test inspection).
    [[nodiscard]] std::vector<LoadResult> consume_completed();

private:
    struct Entry {
        int region_id{0};
        RegionCoord coord{};
        RegionLoadState state{RegionLoadState::None};
        int retry_count{0};
        std::string error_message;
    };

    Entry* find(int region_id);
    Entry* find_or_create(int region_id);

    void try_start_load(Entry& entry);
    void process_completions();
    void process_pending();
    void emit_diagnostic(int region_id, RegionLoadState st,
                         const std::string& msg);

    IStreamingLoader* loader_;
    IStreamingStorage* storage_;
    IStreamingScheduler* scheduler_;

    Budget budget_;
    int in_flight_count_{0};

    std::vector<Entry> entries_;
    std::vector<Entry> pending_;         // FIFO queue of requested loads

    // Thread‑safe: loader callback pushes, update() drains.
    std::mutex completed_mutex_;
    std::vector<LoadResult> completed_;

    // Successful completions moved here at update(), consumed at commit().
    std::vector<LoadResult> ready_for_commit_;

    std::vector<StreamingDiagnostic> diagnostics_;
};

}  // namespace ae::core
