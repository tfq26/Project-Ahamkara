// Streaming executor implementation.
//
// The executor owns a small state machine per region.  Calls to
// request_load() / request_unload() enqueue work; update() advances
// the machine within budgets; commit() makes completed data visible.
//
// Loader callbacks may fire on any thread; they atomically append
// results to a mutex‑protected queue drained during update().

#include "ae/core/streaming_executor.h"

#include <cstring>

namespace ae::core {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

StreamingExecutor::StreamingExecutor(IStreamingLoader* loader,
                                     IStreamingStorage* storage,
                                     IStreamingScheduler* scheduler)
    : loader_(loader), storage_(storage), scheduler_(scheduler) {
}

// ---------------------------------------------------------------------------
// Entry helpers
// ---------------------------------------------------------------------------

StreamingExecutor::Entry* StreamingExecutor::find(int region_id) {
    for (auto& e : entries_) {
        if (e.region_id == region_id)
            return &e;
    }
    return nullptr;
}

StreamingExecutor::Entry* StreamingExecutor::find_or_create(int region_id) {
    Entry* e = find(region_id);
    if (e)
        return e;
    entries_.push_back({region_id, {}, RegionLoadState::None, 0, {}});
    return &entries_.back();
}

// ---------------------------------------------------------------------------
// Public lifecycle
// ---------------------------------------------------------------------------

void StreamingExecutor::request_load(int region_id, RegionCoord coord) {
    Entry* entry = find_or_create(region_id);
    switch (entry->state) {
    case RegionLoadState::Resident:
    case RegionLoadState::Loading:
    case RegionLoadState::Requested:
        return; // no‑op
    default:
        break;
    }

    entry->state = RegionLoadState::Requested;
    entry->coord = coord;
    entry->retry_count = 0;
    entry->error_message.clear();

    pending_.push_back(*entry);
}

void StreamingExecutor::request_unload(int region_id) {
    Entry* entry = find(region_id);
    if (!entry)
        return;
    if (entry->state == RegionLoadState::None ||
        entry->state == RegionLoadState::Evicting) {
        return;
    }

    // Cancel in‑flight load.
    if (entry->state == RegionLoadState::Loading) {
        loader_->cancel_load(region_id);
        --in_flight_count_;
    }

    if (entry->state == RegionLoadState::Resident) {
        entry->state = RegionLoadState::Evicting;
        storage_->evict(region_id);
    } else if (entry->state == RegionLoadState::Requested) {
        // Remove from pending queue.
        auto it = std::find_if(pending_.begin(), pending_.end(),
                               [region_id](const Entry& pe) {
                                   return pe.region_id == region_id;
                               });
        if (it != pending_.end())
            pending_.erase(it);
        entry->state = RegionLoadState::Cancelled;
        emit_diagnostic(region_id, RegionLoadState::Cancelled,
                        "unloaded before start");
    } else if (entry->state == RegionLoadState::Loading) {
        entry->state = RegionLoadState::Cancelled;
        emit_diagnostic(region_id, RegionLoadState::Cancelled,
                        "unloaded during load");
    } else {
        // Failed, Cancelled → reset.
        entry->state = RegionLoadState::None;
    }
}

void StreamingExecutor::update() {
    // 1. Drain thread‑safe completion queue.
    process_completions();

    // 2. Dispatch pending loads that fit within budgets.
    process_pending();
}

void StreamingExecutor::commit() {
    // Drain completions one more time to pick up any results that
    // arrived between the last update() and now.
    process_completions();

    // Commit each successful load to storage.
    for (const auto& r : ready_for_commit_) {
        Entry* entry = find(r.region_id);
        if (!entry || entry->state != RegionLoadState::Loading) {
            continue; // was cancelled between callback and commit
        }

        if (storage_->store(r.region_id, r.data, r.size)) {
            entry->state = RegionLoadState::Resident;
        } else {
            entry->state = RegionLoadState::Failed;
            entry->error_message = "storage store() failed";
            emit_diagnostic(r.region_id, RegionLoadState::Failed,
                            entry->error_message);
        }
    }
    ready_for_commit_.clear();

    // Evicted entries: transition Evicting → None.
    for (auto& e : entries_) {
        if (e.state == RegionLoadState::Evicting) {
            e.state = RegionLoadState::None;
        }
    }
}

void StreamingExecutor::cancel(int region_id) {
    Entry* entry = find(region_id);
    if (!entry)
        return;
    switch (entry->state) {
    case RegionLoadState::None:
    case RegionLoadState::Resident:
    case RegionLoadState::Evicting:
    case RegionLoadState::Cancelled:
        return; // no‑op
    default:
        break;
    }

    RegionLoadState prev = entry->state;
    entry->state = RegionLoadState::Cancelled;
    entry->error_message = "cancelled by client";

    if (prev == RegionLoadState::Loading) {
        loader_->cancel_load(region_id);
        --in_flight_count_;
    } else if (prev == RegionLoadState::Requested) {
        auto it = std::find_if(pending_.begin(), pending_.end(),
                               [region_id](const Entry& pe) {
                                   return pe.region_id == region_id;
                               });
        if (it != pending_.end())
            pending_.erase(it);
    }

    emit_diagnostic(region_id, RegionLoadState::Cancelled,
                    "cancelled by client");
}

void StreamingExecutor::cancel_all() {
    // Collect ids first to avoid iterator invalidation.
    std::vector<int> ids;
    for (const auto& e : entries_) {
        if (e.state == RegionLoadState::Requested ||
            e.state == RegionLoadState::Loading) {
            ids.push_back(e.region_id);
        }
    }
    for (int id : ids)
        cancel(id);
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

RegionLoadState StreamingExecutor::state(int region_id) const {
    for (const auto& e : entries_) {
        if (e.region_id == region_id)
            return e.state;
    }
    return RegionLoadState::None;
}

int StreamingExecutor::failed_count() const {
    int n = 0;
    for (const auto& e : entries_) {
        if (e.state == RegionLoadState::Failed)
            ++n;
    }
    return n;
}

int StreamingExecutor::cancelled_count() const {
    int n = 0;
    for (const auto& e : entries_) {
        if (e.state == RegionLoadState::Cancelled)
            ++n;
    }
    return n;
}

int StreamingExecutor::resident_count() const {
    int n = 0;
    for (const auto& e : entries_) {
        if (e.state == RegionLoadState::Resident)
            ++n;
    }
    return n;
}

std::vector<StreamingDiagnostic> StreamingExecutor::consume_diagnostics() {
    std::vector<StreamingDiagnostic> result;
    result.swap(diagnostics_);
    return result;
}

std::vector<LoadResult> StreamingExecutor::consume_completed() {
    std::vector<LoadResult> result;
    {
        std::lock_guard<std::mutex> lock(completed_mutex_);
        result.swap(completed_);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void StreamingExecutor::try_start_load(Entry& entry) {
    if (in_flight_count_ >= budget_.max_in_flight)
        return;
    if (storage_->stored_bytes() >= budget_.max_storage_bytes)
        return;

    entry.state = RegionLoadState::Loading;
    ++in_flight_count_;

    int rid = entry.region_id;
    loader_->start_load(rid,
                        [this, rid](int region_id, bool ok, const void* data, std::size_t sz) {
                            // Thread‑safe: push result to mutex‑protected queue.
                            (void)rid;
                            std::lock_guard<std::mutex> lock(completed_mutex_);
                            completed_.push_back({region_id, ok, data, sz});
                        });
}

void StreamingExecutor::process_completions() {
    std::vector<LoadResult> batch;
    {
        std::lock_guard<std::mutex> lock(completed_mutex_);
        if (completed_.empty())
            return;
        batch.swap(completed_);
    }

    for (const auto& r : batch) {
        Entry* entry = find(r.region_id);
        if (!entry)
            continue;
        if (entry->state != RegionLoadState::Loading) {
            // Cancelled after callback — decrement in‑flight and skip.
            continue;
        }
        if (!r.success) {
            // Failure during update — handle retry.
            ++entry->retry_count;
            if (entry->retry_count <= budget_.max_retries) {
                entry->state = RegionLoadState::Requested;
                pending_.push_back(*entry);
                emit_diagnostic(r.region_id, RegionLoadState::Requested,
                                "retry " + std::to_string(entry->retry_count));
            } else {
                entry->state = RegionLoadState::Failed;
                entry->error_message = loader_->last_error();
                emit_diagnostic(r.region_id, RegionLoadState::Failed,
                                entry->error_message);
            }
        } else {
            // Successful: hold for commit().
            ready_for_commit_.push_back(r);
        }
        --in_flight_count_;
    }
}

void StreamingExecutor::process_pending() {
    std::vector<Entry> still_pending;
    for (auto& pe : pending_) {
        Entry* entry = find(pe.region_id);
        if (!entry || entry->state != RegionLoadState::Requested) {
            continue; // state changed since queued
        }

        // Check budgets.
        if (in_flight_count_ >= budget_.max_in_flight) {
            still_pending.push_back(pe);
            continue;
        }
        if (storage_->stored_bytes() >= budget_.max_storage_bytes) {
            still_pending.push_back(pe);
            continue;
        }

        try_start_load(*entry);
    }
    pending_.swap(still_pending);
}

void StreamingExecutor::emit_diagnostic(int region_id, RegionLoadState st,
                                        const std::string& msg) {
    diagnostics_.push_back({region_id, st, msg});
}

} // namespace ae::core
