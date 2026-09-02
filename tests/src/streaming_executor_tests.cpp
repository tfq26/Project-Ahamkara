// Streaming executor tests.
//
// Validates StreamingResidencyExecutor: region lifecycle, async loading,
// cancellation, retry, budget enforcement, deterministic ordering, and
// explicit commit barrier.
//
// Uses a FakeRegionLoader that stores pending loads and lets the test
// control when each load completes or fails, simulating out-of-order
// async completions.

#include "ae/runtime/streaming_executor.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// FakeRegionLoader — injectable IRegionLoader for deterministic testing
// ---------------------------------------------------------------------------

class FakeRegionLoader : public ae::IRegionLoader {
public:
    struct PendingLoad {
        int id = -1;
        ae::RegionCoord coord {};
        std::function<void(ae::RegionData)> on_complete;
        std::function<void(const std::string&)> on_fail;
    };

    ~FakeRegionLoader() override = default;

    int start_load(ae::RegionCoord coord,
                   std::function<void(ae::RegionData)> on_complete,
                   std::function<void(const std::string&)> on_fail) override
    {
        const int id = next_handle_++;
        pending_.push_back({id, coord, std::move(on_complete), std::move(on_fail)});
        loading_coords_.insert(coord);
        return id;
    }

    void cancel_load(int handle) override {
        for (auto it = pending_.begin(); it != pending_.end(); ++it) {
            if (it->id == handle) {
                loading_coords_.erase(it->coord);
                cancelled_.push_back(it->coord);
                pending_.erase(it);
                return;
            }
        }
    }

    int in_flight_count() const override {
        return static_cast<int>(pending_.size());
    }

    // -- Test helpers -------------------------------------------------------

    /// Number of pending (in-flight) loads.
    int pending_count() const { return static_cast<int>(pending_.size()); }

    /// Whether a specific region has an in-flight load.
    bool is_loading(ae::RegionCoord coord) const {
        return loading_coords_.count(coord) > 0;
    }

    /// Whether a specific region was cancelled.
    bool was_cancelled(ae::RegionCoord coord) const {
        return std::find(cancelled_.begin(), cancelled_.end(), coord) != cancelled_.end();
    }

    /// Complete a specific pending load (invokes on_complete).
    void complete_pending(ae::RegionCoord coord, ae::RegionData data) {
        for (auto it = pending_.begin(); it != pending_.end(); ++it) {
            if (it->coord == coord) {
                loading_coords_.erase(coord);
                auto cb = std::move(it->on_complete);
                pending_.erase(it);
                if (cb) cb(std::move(data));
                return;
            }
        }
    }

    /// Fail a specific pending load (invokes on_fail).
    void fail_pending(ae::RegionCoord coord, const std::string& error) {
        for (auto it = pending_.begin(); it != pending_.end(); ++it) {
            if (it->coord == coord) {
                loading_coords_.erase(coord);
                auto cb = std::move(it->on_fail);
                pending_.erase(it);
                if (cb) cb(error);
                return;
            }
        }
    }

    /// Complete all pending loads in a caller-specified order.
    /// Coords not in `order` are skipped (remain pending).
    void complete_in_order(const std::vector<ae::RegionCoord>& order) {
        for (const auto& coord : order) {
            complete_pending(coord, std::make_shared<const std::vector<char>>());
        }
    }

    /// Complete all remaining pending loads in an arbitrary but stable order
    /// (sorted by (cy, cx) then by id for determinism).
    void complete_all_remaining() {
        // Collect all unique coords first (to avoid iterator invalidation).
        std::vector<ae::RegionCoord> coords;
        for (const auto& p : pending_) {
            coords.push_back(p.coord);
        }
        // Stable sort by coord.
        std::stable_sort(coords.begin(), coords.end(),
            [](ae::RegionCoord a, ae::RegionCoord b) {
                if (a.cy != b.cy) return a.cy < b.cy;
                return a.cx < b.cx;
            });
        for (const auto& coord : coords) {
            complete_pending(coord, std::make_shared<const std::vector<char>>());
        }
    }

    /// Fail all remaining pending loads.
    void fail_all_remaining(const std::string& error) {
        while (!pending_.empty()) {
            auto p = std::move(pending_.back());
            pending_.pop_back();
            loading_coords_.erase(p.coord);
            if (p.on_fail) p.on_fail(error);
        }
    }

    /// Get the set of coords that are currently in-flight.
    std::set<ae::RegionCoord, bool(*)(ae::RegionCoord, ae::RegionCoord)>
    in_flight_set() const {
        std::set<ae::RegionCoord, bool(*)(ae::RegionCoord, ae::RegionCoord)> s(
            [](ae::RegionCoord a, ae::RegionCoord b) {
                if (a.cy != b.cy) return a.cy < b.cy;
                return a.cx < b.cx;
            });
        for (const auto& p : pending_) {
            s.insert(p.coord);
        }
        return s;
    }

    /// Clear all state (for reuse).
    void reset() {
        pending_.clear();
        cancelled_.clear();
        loading_coords_.clear();
        next_handle_ = 1;
    }

private:
    int next_handle_ = 1;
    std::vector<PendingLoad> pending_;
    std::vector<ae::RegionCoord> cancelled_;
    std::set<ae::RegionCoord> loading_coords_;
};

// ---------------------------------------------------------------------------
// Test utilities
// ---------------------------------------------------------------------------

namespace {

int fail(const std::string& msg) {
    std::cerr << "streaming_executor_tests FAILED: " << msg << '\n';
    return 1;
}

auto make_data(const std::string& content) {
    auto vec = std::make_shared<std::vector<char>>(content.begin(), content.end());
    return ae::RegionData(std::move(vec));
}

ae::RegionCoord rc(int cx, int cy) { return {cx, cy}; }

// ---------------------------------------------------------------------------
// Test: Basic load → tick → commit lifecycle
// ---------------------------------------------------------------------------

int test_basic_lifecycle() {
    auto fl = std::make_unique<FakeRegionLoader>();
    auto* fl_ptr = fl.get();
    ae::StreamingResidencyExecutor exec;
    exec.set_loader(std::move(fl));

    // Request a load for region (1,1).
    exec.process_transitions({{rc(1, 1), true}});
    if (exec.state(rc(1, 1)) != ae::RegionState::Requested)
        return fail("after process_transitions(load), state should be Requested");

    // Tick dispatches the load.
    exec.tick();
    if (exec.state(rc(1, 1)) != ae::RegionState::Loading)
        return fail("after tick(), state should be Loading");
    if (!fl_ptr->is_loading(rc(1, 1)))
        return fail("loader should have a pending load for (1,1)");
    if (exec.in_flight_count() != 1)
        return fail("in_flight_count should be 1");

    // Complete the load.
    auto data = make_data("hello");
    fl_ptr->complete_pending(rc(1, 1), data);

    // Tick picks up the completion.
    exec.tick();
    // State should still be Loading (pending_data filled, not yet committed).
    if (exec.state(rc(1, 1)) != ae::RegionState::Loading)
        return fail("after load complete + tick, state should still be Loading (pre-commit)");
    if (exec.in_flight_count() != 0)
        return fail("in_flight_count should be 0 after completion");

    // Commit makes it Resident.
    exec.commit();
    if (exec.state(rc(1, 1)) != ae::RegionState::Resident)
        return fail("after commit(), state should be Resident");
    if (exec.resident_count() != 1)
        return fail("resident_count should be 1");
    if (!exec.data(rc(1, 1)))
        return fail("data() should return non-null for resident region");
    if (exec.resident_memory_bytes() != data->size())
        return fail("resident_memory_bytes should match loaded data size");

    return 0;
}

// ---------------------------------------------------------------------------
// Test: Load and unload (cancel in-flight)
// ---------------------------------------------------------------------------

int test_cancel_in_flight_load() {
    auto fl = std::make_unique<FakeRegionLoader>();
    auto* fl_ptr = fl.get();
    ae::StreamingResidencyExecutor exec;
    exec.set_loader(std::move(fl));

    // Start load.
    exec.process_transitions({{rc(2, 2), true}});
    exec.tick();
    if (exec.state(rc(2, 2)) != ae::RegionState::Loading)
        return fail("should be Loading");

    // Cancel before completion.
    exec.process_transitions({{rc(2, 2), false}});  // unload
    if (exec.state(rc(2, 2)) != ae::RegionState::Cancelled)
        return fail("should be Cancelled after unload transition");
    if (!fl_ptr->was_cancelled(rc(2, 2)))
        return fail("loader should have been asked to cancel");
    if (exec.in_flight_count() != 0)
        return fail("in_flight should be 0 after cancel");

    // Simulate late completion (should be discarded by loader cancel
    // since the pending load was removed on cancel).
    fl_ptr->complete_pending(rc(2, 2), make_data("late"));
    exec.tick();
    // Should be None after cleanup.
    if (exec.state(rc(2, 2)) != ae::RegionState::None)
        return fail("late completion after cancel should result in None state");

    // Verify region is not resident.
    if (exec.data(rc(2, 2)))
        return fail("cancelled region should not have data");

    return 0;
}

// ---------------------------------------------------------------------------
// Test: Load failure with retry
// ---------------------------------------------------------------------------

int test_load_failure_retry() {
    auto fl = std::make_unique<FakeRegionLoader>();
    auto* fl_ptr = fl.get();
    ae::StreamingResidencyExecutor exec({
        .max_in_flight_loads = 4,
        .max_memory_bytes = 256ULL * 1024ULL * 1024ULL,
        .max_retry_count = 3,
    });
    exec.set_loader(std::move(fl));

    // Start load.
    exec.process_transitions({{rc(0, 0), true}});
    exec.tick();
    if (exec.state(rc(0, 0)) != ae::RegionState::Loading)
        return fail("should be Loading");

    // Fail the load.
    fl_ptr->fail_pending(rc(0, 0), "disk error");
    exec.tick();

    // Tick should have retried (retry 1 → Requested → Loading).
    if (exec.state(rc(0, 0)) != ae::RegionState::Loading)
        return fail("after first failure + tick, should retry to Loading");

    // Fail again.
    fl_ptr->fail_pending(rc(0, 0), "disk error again");
    exec.tick();
    if (exec.state(rc(0, 0)) != ae::RegionState::Loading)
        return fail("after second failure + tick, should retry to Loading");

    // Fail a third time (retry_count reaches max_retry_count=3).
    fl_ptr->fail_pending(rc(0, 0), "disk error thrice");
    exec.tick();
    // After third failure, no more retries, should be Failed.
    if (exec.state(rc(0, 0)) != ae::RegionState::Failed)
        return fail("after max retries, state should be Failed");

    return 0;
}

// ---------------------------------------------------------------------------
// Test: Retry eventually succeeds
// ---------------------------------------------------------------------------

int test_retry_eventually_succeeds() {
    auto fl = std::make_unique<FakeRegionLoader>();
    auto* fl_ptr = fl.get();
    ae::StreamingResidencyExecutor exec({
        .max_in_flight_loads = 4,
        .max_memory_bytes = 256ULL * 1024ULL * 1024ULL,
        .max_retry_count = 3,
    });
    exec.set_loader(std::move(fl));

    // Start load.
    exec.process_transitions({{rc(5, 5), true}});
    exec.tick();
    if (exec.state(rc(5, 5)) != ae::RegionState::Loading)
        return fail("should be Loading");

    // Fail twice.
    fl_ptr->fail_pending(rc(5, 5), "transient error");
    exec.tick();
    fl_ptr->fail_pending(rc(5, 5), "transient error again");
    exec.tick();

    // Success on third attempt.
    fl_ptr->complete_pending(rc(5, 5), make_data("success after retry"));
    exec.tick();
    exec.commit();

    if (exec.state(rc(5, 5)) != ae::RegionState::Resident)
        return fail("after retry + success + commit, state should be Resident");
    if (exec.resident_count() != 1)
        return fail("resident_count should be 1");

    return 0;
}

// ---------------------------------------------------------------------------
// Test: In-flight budget enforcement
// ---------------------------------------------------------------------------

int test_in_flight_budget() {
    constexpr int kMaxInFlight = 2;

    auto fl = std::make_unique<FakeRegionLoader>();
    auto* fl_ptr = fl.get();
    ae::StreamingResidencyExecutor exec({
        .max_in_flight_loads = kMaxInFlight,
        .max_memory_bytes = 256ULL * 1024ULL * 1024ULL,
        .max_retry_count = 3,
    });
    exec.set_loader(std::move(fl));

    // Request 5 loads.
    std::vector<ae::RegionTransition> transitions;
    for (int i = 0; i < 5; ++i) {
        transitions.push_back({rc(i, 0), true});
    }
    exec.process_transitions(transitions);

    // Tick should only dispatch <= kMaxInFlight.
    exec.tick();
    if (exec.in_flight_count() != kMaxInFlight)
        return fail("in_flight_count should be capped at " + std::to_string(kMaxInFlight));
    if (fl_ptr->pending_count() != kMaxInFlight)
        return fail("loader should have exactly " + std::to_string(kMaxInFlight) + " pending loads");

    // Verify only first kMaxInFlight are loading, rest are Requested.
    for (int i = 0; i < kMaxInFlight; ++i) {
        if (exec.state(rc(i, 0)) != ae::RegionState::Loading)
            return fail("region " + std::to_string(i) + " should be Loading");
    }
    for (int i = kMaxInFlight; i < 5; ++i) {
        if (exec.state(rc(i, 0)) != ae::RegionState::Requested)
            return fail("region " + std::to_string(i) + " should still be Requested (budget)");
    }

    // Complete one load → tick should dispatch next (region 2, which is
    // next in insertion order after 0 and 1).
    fl_ptr->complete_pending(rc(0, 0), make_data("a"));
    exec.tick();
    if (exec.in_flight_count() != kMaxInFlight)
        return fail("after completing one, in_flight should be back at " + std::to_string(kMaxInFlight));
    if (exec.state(rc(2, 0)) != ae::RegionState::Loading)
        return fail("region 2 should now be Loading (dispatched after slot freed)");
    // Region 3 should still be Requested (budget capped again).
    if (exec.state(rc(3, 0)) != ae::RegionState::Requested)
        return fail("region 3 should still be Requested (budget)");

    return 0;
}

// ---------------------------------------------------------------------------
// Test: Memory budget enforcement at commit
// ---------------------------------------------------------------------------

int test_memory_budget_commit() {
    constexpr std::size_t kMaxMem = 100;

    auto fl = std::make_unique<FakeRegionLoader>();
    auto* fl_ptr = fl.get();
    ae::StreamingResidencyExecutor exec({
        .max_in_flight_loads = 10,
        .max_memory_bytes = kMaxMem,
        .max_retry_count = 3,
    });
    exec.set_loader(std::move(fl));

    // Load three regions whose data totals 150 bytes (> 100).
    exec.process_transitions({{rc(0, 0), true}, {rc(1, 0), true}, {rc(2, 0), true}});
    exec.tick();

    // Complete all loads.
    fl_ptr->complete_pending(rc(0, 0), make_data(std::string(60, 'x')));
    fl_ptr->complete_pending(rc(1, 0), make_data(std::string(60, 'y')));
    fl_ptr->complete_pending(rc(2, 0), make_data(std::string(30, 'z')));
    exec.tick();

    // commit() should fail because 60+60+30 = 150 > 100.
    exec.commit();
    if (exec.resident_count() != 0)
        return fail("commit should be rejected when memory budget exceeded");

    // Evict one region to free budget.
    exec.process_transitions({{rc(0, 0), false}});  // unload
    exec.tick();  // completes eviction

    // Reload (0,0) with smaller data.
    exec.process_transitions({{rc(0, 0), true}});
    exec.tick();
    fl_ptr->complete_pending(rc(0, 0), make_data(std::string(40, 'a')));
    exec.tick();

    // Now pending: (0,0)=40, (1,0)=60, (2,0)=30 = 130 > 100.
    exec.commit();
    if (exec.resident_count() != 0)
        return fail("commit should still be rejected (40+60+30 = 130 > 100)");

    // Unload (2,0) as well.
    exec.process_transitions({{rc(2, 0), false}});
    exec.tick();

    // Now pending: (0,0)=40, (1,0)=60 = 100 <= 100.
    exec.commit();
    if (exec.resident_count() != 2)
        return fail("commit should succeed now (40+60 = 100 <= 100)");
    if (exec.resident_memory_bytes() != 100)
        return fail("resident_memory_bytes should be 100");

    return 0;
}

// ---------------------------------------------------------------------------
// Test: Deterministic ordering under reordered completions
// ---------------------------------------------------------------------------

int test_deterministic_ordering() {
    // Run the same scenario 3 times with different completion orders.
    // The final set of resident regions should be identical.
    constexpr int kIterations = 5;

    for (int iter = 0; iter < kIterations; ++iter) {
        auto fl = std::make_unique<FakeRegionLoader>();
        auto* fl_ptr = fl.get();
        ae::StreamingResidencyExecutor exec({
            .max_in_flight_loads = 10,
            .max_memory_bytes = 256ULL * 1024ULL * 1024ULL,
            .max_retry_count = 3,
        });
        exec.set_loader(std::move(fl));

        // Request loads for 6 regions.
        const std::vector<ae::RegionCoord> coords = {
            rc(0, 0), rc(1, 0), rc(2, 0), rc(0, 1), rc(1, 1), rc(2, 1)
        };
        std::vector<ae::RegionTransition> transitions;
        for (const auto& c : coords) {
            transitions.push_back({c, true});
        }
        exec.process_transitions(transitions);
        exec.tick();

        // Complete in different shuffled orders per iteration.
        // Use a predictable permutation per iteration.
        std::vector<ae::RegionCoord> order = coords;
        if (iter == 0) {
            // Forward order.
        } else if (iter == 1) {
            // Reverse order.
            std::reverse(order.begin(), order.end());
        } else if (iter == 2) {
            // Odd regions first.
            std::stable_sort(order.begin(), order.end(),
                [](ae::RegionCoord a, ae::RegionCoord b) {
                    bool a_odd = (a.cx + a.cy) % 2 != 0;
                    bool b_odd = (b.cx + b.cy) % 2 != 0;
                    if (a_odd != b_odd) return a_odd > b_odd;
                    if (a.cy != b.cy) return a.cy < b.cy;
                    return a.cx < b.cx;
                });
        } else {
            // Rotating order.
            std::rotate(order.begin(), order.begin() + iter, order.end());
        }

        fl_ptr->complete_in_order(order);
        exec.tick();

        // Unload region (0,0) and (2,1) before commit.
        exec.process_transitions({{rc(0, 0), false}, {rc(2, 1), false}});
        exec.tick();

        // Commit remaining.
        exec.commit();

        // Verify final state is deterministic regardless of completion order.
        // Resident: (1,0), (2,0), (0,1), (1,1) = 4 regions.
        if (exec.resident_count() != 4)
            return fail("iter " + std::to_string(iter) + ": expected 4 resident regions, got " +
                        std::to_string(exec.resident_count()));

        if (exec.state(rc(1, 0)) != ae::RegionState::Resident)
            return fail("iter " + std::to_string(iter) + ": (1,0) should be Resident");
        if (exec.state(rc(2, 0)) != ae::RegionState::Resident)
            return fail("iter " + std::to_string(iter) + ": (2,0) should be Resident");
        if (exec.state(rc(0, 1)) != ae::RegionState::Resident)
            return fail("iter " + std::to_string(iter) + ": (0,1) should be Resident");
        if (exec.state(rc(1, 1)) != ae::RegionState::Resident)
            return fail("iter " + std::to_string(iter) + ": (1,1) should be Resident");

        // Cancelled or None for the unloaded regions.
        if (exec.state(rc(0, 0)) != ae::RegionState::None)
            return fail("iter " + std::to_string(iter) + ": (0,0) should be None (cancelled before commit)");
        if (exec.state(rc(2, 1)) != ae::RegionState::Cancelled &&
            exec.state(rc(2, 1)) != ae::RegionState::None)
            return fail("iter " + std::to_string(iter) + ": (2,1) should be Cancelled or None");
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Test: Resident region gets evicted
// ---------------------------------------------------------------------------

int test_resident_eviction() {
    auto fl = std::make_unique<FakeRegionLoader>();
    auto* fl_ptr = fl.get();
    ae::StreamingResidencyExecutor exec;
    exec.set_loader(std::move(fl));

    // Load and commit a region.
    exec.process_transitions({{rc(3, 3), true}});
    exec.tick();
    fl_ptr->complete_pending(rc(3, 3), make_data("resident data"));
    exec.tick();
    exec.commit();

    if (exec.state(rc(3, 3)) != ae::RegionState::Resident)
        return fail("region should be Resident after commit");
    if (exec.resident_count() != 1)
        return fail("should have 1 resident region");

    // Unload it.
    exec.process_transitions({{rc(3, 3), false}});
    if (exec.state(rc(3, 3)) != ae::RegionState::Evicting)
        return fail("state should be Evicting after unload transition");

    // Tick completes eviction.
    exec.tick();
    if (exec.state(rc(3, 3)) != ae::RegionState::None)
        return fail("state should be None after eviction completes");
    if (exec.resident_count() != 0)
        return fail("should have 0 resident regions after eviction");
    if (exec.data(rc(3, 3)))
        return fail("evicted region should have null data");
    if (exec.resident_memory_bytes() != 0)
        return fail("resident_memory_bytes should be 0 after eviction");

    return 0;
}

// ---------------------------------------------------------------------------
// Test: Cancel before dispatch (Requested → Cancelled)
// ---------------------------------------------------------------------------

int test_cancel_before_dispatch() {
    auto fl = std::make_unique<FakeRegionLoader>();
    ae::StreamingResidencyExecutor exec({
        .max_in_flight_loads = 1,
        .max_memory_bytes = 256ULL * 1024ULL * 1024ULL,
        .max_retry_count = 3,
    });
    exec.set_loader(std::move(fl));

    // Request 2 loads but budget is 1. Second stays Requested.
    exec.process_transitions({{rc(0, 0), true}, {rc(1, 0), true}});
    exec.tick();
    // (0,0) is Loading, (1,0) is Requested.

    // Cancel (1,0) before it dispatches.
    exec.process_transitions({{rc(1, 0), false}});
    if (exec.state(rc(1, 0)) != ae::RegionState::None)
        return fail("cancelled-before-dispatch region should be None");

    return 0;
}

// ---------------------------------------------------------------------------
// Test: Empty transitions produces no state changes
// ---------------------------------------------------------------------------

int test_empty_transitions() {
    auto fl = std::make_unique<FakeRegionLoader>();
    ae::StreamingResidencyExecutor exec;
    exec.set_loader(std::move(fl));

    exec.process_transitions({});
    exec.tick();
    exec.commit();

    if (exec.tracked_region_count() != 0)
        return fail("no regions should be tracked after empty transitions");
    if (exec.in_flight_count() != 0)
        return fail("in_flight should be 0");
    if (exec.resident_count() != 0)
        return fail("resident_count should be 0");

    return 0;
}

// ---------------------------------------------------------------------------
// Test: Duplicate load request is idempotent
// ---------------------------------------------------------------------------

int test_duplicate_load_request() {
    auto fl = std::make_unique<FakeRegionLoader>();
    auto* fl_ptr = fl.get();
    ae::StreamingResidencyExecutor exec;
    exec.set_loader(std::move(fl));

    // Request same region twice.
    exec.process_transitions({{rc(0, 0), true}});
    exec.process_transitions({{rc(0, 0), true}});

    exec.tick();
    if (exec.in_flight_count() != 1)
        return fail("duplicate load should not double-dispatch");

    // Complete.
    fl_ptr->complete_pending(rc(0, 0), make_data("data"));
    exec.tick();
    exec.commit();

    if (exec.state(rc(0, 0)) != ae::RegionState::Resident)
        return fail("region should become Resident after duplicate load");

    return 0;
}

// ---------------------------------------------------------------------------
// Test: Commit with no pending data is a no-op
// ---------------------------------------------------------------------------

int test_commit_no_pending_data() {
    auto fl = std::make_unique<FakeRegionLoader>();
    ae::StreamingResidencyExecutor exec;
    exec.set_loader(std::move(fl));

    // Create a loading region but don't complete it.
    exec.process_transitions({{rc(0, 0), true}});
    exec.tick();

    // Commit with nothing pending.
    exec.commit();
    if (exec.state(rc(0, 0)) != ae::RegionState::Loading)
        return fail("region should still be Loading after empty commit");
    if (exec.resident_count() != 0)
        return fail("no regions should be resident after empty commit");

    return 0;
}

// ---------------------------------------------------------------------------
// Test: Multiple rounds of transitions/tick/commit
// ---------------------------------------------------------------------------

int test_multiple_rounds() {
    auto fl = std::make_unique<FakeRegionLoader>();
    auto* fl_ptr = fl.get();
    ae::StreamingResidencyExecutor exec({
        .max_in_flight_loads = 2,
        .max_memory_bytes = 256ULL * 1024ULL * 1024ULL,
        .max_retry_count = 3,
    });
    exec.set_loader(std::move(fl));

    // Round 1: request 4 regions, only 2 dispatch.
    exec.process_transitions({{rc(0, 0), true}, {rc(1, 0), true},
                              {rc(2, 0), true}, {rc(3, 0), true}});
    exec.tick();
    if (exec.in_flight_count() != 2)
        return fail("round 1: should have 2 in-flight");

    // Complete (0,0) and (1,0).
    fl_ptr->complete_pending(rc(0, 0), make_data("a"));
    fl_ptr->complete_pending(rc(1, 0), make_data("b"));
    exec.tick();

    // Commit round 1.
    exec.commit();
    if (exec.state(rc(0, 0)) != ae::RegionState::Resident)
        return fail("round 1: (0,0) should be Resident");
    if (exec.state(rc(1, 0)) != ae::RegionState::Resident)
        return fail("round 1: (1,0) should be Resident");

    // Round 2: (2,0) and (3,0) should have been dispatched by tick.
    if (exec.state(rc(2, 0)) != ae::RegionState::Loading)
        return fail("round 2: (2,0) should be Loading");
    if (exec.state(rc(3, 0)) != ae::RegionState::Loading)
        return fail("round 2: (3,0) should be Loading");

    // Complete and commit.
    fl_ptr->complete_pending(rc(2, 0), make_data("c"));
    fl_ptr->complete_pending(rc(3, 0), make_data("d"));
    exec.tick();
    exec.commit();

    if (exec.resident_count() != 4)
        return fail("after two rounds, should have 4 resident regions");

    // Unload (0,0) in round 3.
    exec.process_transitions({{rc(0, 0), false}});
    exec.tick();

    if (exec.resident_count() != 3)
        return fail("after eviction, should have 3 resident regions");
    if (exec.data(rc(0, 0)))
        return fail("evicted region should have null data");

    return 0;
}

// ---------------------------------------------------------------------------
// Test: Failed region records stable diagnostic message
// ---------------------------------------------------------------------------

int test_failure_diagnostic() {
    auto fl = std::make_unique<FakeRegionLoader>();
    auto* fl_ptr = fl.get();
    ae::StreamingResidencyExecutor exec({
        .max_in_flight_loads = 4,
        .max_memory_bytes = 256ULL * 1024ULL * 1024ULL,
        .max_retry_count = 1,  // Only 1 retry to test failure quickly.
    });
    exec.set_loader(std::move(fl));

    exec.process_transitions({{rc(7, 7), true}});
    exec.tick();
    // Fail the load — with max_retry_count=1, no retry available.
    fl_ptr->fail_pending(rc(7, 7), "I/O error: disk not ready");
    exec.tick();

    // State should be Failed (no retries with max_retry_count=1).
    if (exec.state(rc(7, 7)) != ae::RegionState::Failed)
        return fail("after failure with max_retry_count=1, state should be Failed");

    // Failed region should not be resident and not have data.
    if (exec.data(rc(7, 7)))
        return fail("failed region should not have data");

    return 0;
}

// ---------------------------------------------------------------------------
// Test: Transition ignored for non-existent region on unload
// ---------------------------------------------------------------------------

int test_unload_nonexistent_region() {
    auto fl = std::make_unique<FakeRegionLoader>();
    ae::StreamingResidencyExecutor exec;
    exec.set_loader(std::move(fl));

    // Unload a region that was never loaded.
    exec.process_transitions({{rc(99, 99), false}});
    exec.tick();
    exec.commit();

    if (exec.tracked_region_count() != 0)
        return fail("no region should be tracked after unloading non-existent region");
    if (exec.resident_count() != 0)
        return fail("no resident regions expected");

    return 0;
}

// ---------------------------------------------------------------------------
// Test: All pending loads eventually complete or fail deterministically
// ---------------------------------------------------------------------------

int test_all_complete_deterministic() {
    // Test that completing all pending loads in a different order
    // still results in the same final state map.
    for (int phase = 0; phase < 3; ++phase) {
        auto fl = std::make_unique<FakeRegionLoader>();
        auto* fl_ptr = fl.get();
        ae::StreamingResidencyExecutor exec;
        exec.set_loader(std::move(fl));

        exec.process_transitions({{rc(0, 0), true}, {rc(1, 0), true}, {rc(2, 0), true}});
        exec.tick();

        // Phase 0: complete in forward order.
        // Phase 1: complete in reverse order.
        // Phase 2: fail all, then retry and complete in mixed order.
        if (phase == 0) {
            fl_ptr->complete_pending(rc(0, 0), make_data("a"));
            fl_ptr->complete_pending(rc(1, 0), make_data("b"));
            fl_ptr->complete_pending(rc(2, 0), make_data("c"));
        } else if (phase == 1) {
            fl_ptr->complete_pending(rc(2, 0), make_data("c"));
            fl_ptr->complete_pending(rc(1, 0), make_data("b"));
            fl_ptr->complete_pending(rc(0, 0), make_data("a"));
        } else {
            fl_ptr->fail_pending(rc(0, 0), "fail");
            fl_ptr->fail_pending(rc(1, 0), "fail");
            fl_ptr->fail_pending(rc(2, 0), "fail");
            exec.tick();  // retries all
            fl_ptr->complete_pending(rc(2, 0), make_data("c"));
            fl_ptr->complete_pending(rc(0, 0), make_data("a"));
            fl_ptr->complete_pending(rc(1, 0), make_data("b"));
        }

        exec.tick();
        exec.commit();

        if (exec.resident_count() != 3)
            return fail("phase " + std::to_string(phase) + ": expected 3 resident regions");
        if (exec.state(rc(0, 0)) != ae::RegionState::Resident)
            return fail("phase " + std::to_string(phase) + ": (0,0) should be Resident");
        if (exec.state(rc(1, 0)) != ae::RegionState::Resident)
            return fail("phase " + std::to_string(phase) + ": (1,0) should be Resident");
        if (exec.state(rc(2, 0)) != ae::RegionState::Resident)
            return fail("phase " + std::to_string(phase) + ": (2,0) should be Resident");
    }
    return 0;
}

}  // namespace

// =============================================================================
// main()
// =============================================================================

int main() {
    // Basic lifecycle
    if (int rc = test_basic_lifecycle()) return rc;

    // Cancellation
    if (int rc = test_cancel_in_flight_load()) return rc;
    if (int rc = test_cancel_before_dispatch()) return rc;

    // Retry
    if (int rc = test_load_failure_retry()) return rc;
    if (int rc = test_retry_eventually_succeeds()) return rc;
    if (int rc = test_failure_diagnostic()) return rc;

    // Budget
    if (int rc = test_in_flight_budget()) return rc;
    if (int rc = test_memory_budget_commit()) return rc;

    // Eviction
    if (int rc = test_resident_eviction()) return rc;

    // Edge cases
    if (int rc = test_empty_transitions()) return rc;
    if (int rc = test_duplicate_load_request()) return rc;
    if (int rc = test_commit_no_pending_data()) return rc;
    if (int rc = test_unload_nonexistent_region()) return rc;

    // Multi-round
    if (int rc = test_multiple_rounds()) return rc;

    // Deterministic ordering (the marquee tests)
    if (int rc = test_deterministic_ordering()) return rc;
    if (int rc = test_all_complete_deterministic()) return rc;

    std::cout << "streaming_executor_tests: all 17 tests passed\n";
    return 0;
}
