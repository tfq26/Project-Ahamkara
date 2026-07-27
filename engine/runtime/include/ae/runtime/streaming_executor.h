#pragma once

// Streaming residency executor.
//
// Consumes RegionTransition objects produced by ResidencyManager and drives
// asynchronous loads and unloads through an injectable IRegionLoader interface.
//
// State machine per region:
//   None ──[process_transitions(load)]──> Requested
//   Requested ──[tick() dispatch]──> Loading
//   Loading ──[loader on_complete]──> Loading (pending_data filled)
//   Loading ──[commit()]──> Resident
//   Loading ──[loader on_fail]──> Failed
//   Failed ──[tick() retry]──> Requested  (if retry_count < max_retry_count)
//   Loading ──[process_transitions(unload)]──> Cancelled
//   Cancelled ──[tick() cleanup]──> None
//   Failed ──[max retries exhausted]──> None
//   Resident ──[process_transitions(unload)]──> Evicting
//   Evicting ──[tick() evict]──> None
//
// Budgets (in-flight count, memory bytes) are enforced during tick() dispatch
// and commit(). Loaded data only becomes visible to the simulation at commit().

#include "ae/core/residency_manager.h"  // RegionCoord, RegionTransition

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ae {

// Re-export core residency types into the ae namespace for convenience.
using core::RegionCoord;
using core::RegionTransition;

/// Opaque container for committed region payload data.
/// Immutable once assigned — held by shared_ptr for lightweight copying.
using RegionData = std::shared_ptr<const std::vector<char>>;

/// Region lifecycle states visible to the executor's caller.
enum class RegionState : std::uint8_t {
    None = 0,
    Requested,   ///< Load requested; not yet dispatched to loader.
    Loading,     ///< Async load in progress.
    Resident,    ///< Fully loaded and committed; data is simulation-visible.
    Failed,      ///< Load attempt failed; eligible for retry.
    Cancelled,   ///< Load was cancelled before completion; data discarded.
    Evicting,    ///< Previously resident region being evicted.
};

/// Injectable interface for asynchronous region loading.
///
/// Implementations are responsible for their own thread/worker management
/// (e.g., via ae::JobSystem).  The executor calls start_load() from its
/// tick() method and cancel_load() when a region is no longer needed.
///
/// Thread safety: start_load / cancel_load / in_flight_count may be called
/// from any thread.  on_complete / on_fail callbacks are invoked from the
/// loader's own context and must not block.
class IRegionLoader {
public:
    virtual ~IRegionLoader() = default;

    /// Start an asynchronous load for a region.
    /// @param coord        Which region to load.
    /// @param on_complete  Invoked exactly once if the load succeeds.
    /// @param on_fail      Invoked exactly once if the load fails.
    /// @return A non-negative handle that can be passed to cancel_load().
    virtual int start_load(
        RegionCoord coord,
        std::function<void(RegionData)> on_complete,
        std::function<void(const std::string&)> on_fail) = 0;

    /// Cancel a previously started load.  After this returns the executor
    /// guarantees that neither on_complete nor on_fail will be invoked for
    /// this handle.
    virtual void cancel_load(int handle) = 0;

    /// Number of loads currently in flight.
    virtual int in_flight_count() const = 0;
};

/// Streaming executor: drives ResidencyManager transitions through an async
/// loader, enforces budgets, and exposes an explicit commit() barrier.
///
/// Usage:
///   1. Construct with config.
///   2. set_loader(...) with an IRegionLoader implementation.
///   3. Each frame:
///      a. Call process_transitions() with ResidencyManager::consume_pending().
///      b. Call tick() to dispatch loads and process completions.
///      c. Call commit() to make loaded data visible.
class StreamingResidencyExecutor {
public:
    struct Config {
        int max_in_flight_loads = 4;                         ///< Hard cap on concurrent loads.
        std::size_t max_memory_bytes = 256ULL * 1024ULL * 1024ULL;  ///< Hard cap on resident payload bytes.
        int max_retry_count = 3;                              ///< Load retries before giving up.
    };

    StreamingResidencyExecutor();
    explicit StreamingResidencyExecutor(Config config);
    ~StreamingResidencyExecutor();

    StreamingResidencyExecutor(const StreamingResidencyExecutor&) = delete;
    StreamingResidencyExecutor& operator=(const StreamingResidencyExecutor&) = delete;

    /// Inject the loader.  Must be called before the first tick().
    void set_loader(std::unique_ptr<IRegionLoader> loader);

    /// Process a batch of transitions (typically from ResidencyManager::consume_pending()).
    /// Load transitions → Requested.  Unload transitions → Cancelled/Evicting.
    void process_transitions(const std::vector<RegionTransition>& transitions);

    /// Tick: dispatch requested loads (within budget), drain completion queue,
    /// handle retries, and complete evictions.
    void tick();

    /// Commit barrier: all fully-loaded regions transition Loading → Resident.
    /// After this call, committed data is visible via data() and state().
    void commit();

    // -- Queries ---------------------------------------------------------------

    /// Current state of a region.
    RegionState state(RegionCoord coord) const;

    /// Committed data for a Resident region (nullptr for non-resident regions).
    RegionData data(RegionCoord coord) const;

    /// Number of loads currently dispatched to the loader.
    int in_flight_count() const { return in_flight_count_; }

    /// Number of regions in Resident state.
    int resident_count() const;

    /// Total byte size of all resident region payloads.
    std::size_t resident_memory_bytes() const { return resident_memory_bytes_; }

    /// Number of regions tracked by the executor (any non-None state).
    int tracked_region_count() const { return static_cast<int>(regions_.size()); }

private:
    struct RegionEntry {
        RegionCoord coord {};
        RegionState state = RegionState::None;
        RegionData data;           ///< Committed data (visible after commit).
        RegionData pending_data;   ///< Fully loaded but awaiting commit().
        std::string error_message; ///< Stable diagnostic from last failure.
        int load_handle = -1;      ///< Handle returned by loader->start_load().
        int retry_count = 0;       ///< Number of failed load attempts.
    };

    RegionEntry* find_entry(RegionCoord coord);
    RegionEntry& get_or_create_entry(RegionCoord coord);

    void try_dispatch_load(RegionEntry& entry);
    void start_eviction(RegionEntry& entry);
    void complete_eviction(RegionEntry& entry);
    void discard_entry(RegionEntry& entry);

    struct PendingCompletion {
        RegionCoord coord;
        bool success;
        RegionData data;
        std::string error;
    };

    Config config_;
    std::unique_ptr<IRegionLoader> loader_;
    std::vector<RegionEntry> regions_;
    int in_flight_count_ = 0;
    std::size_t resident_memory_bytes_ = 0;

    std::vector<PendingCompletion> pending_completions_;
    std::mutex completion_mutex_;
};

}  // namespace ae
