#include "ae/runtime/streaming_executor.h"

#include <algorithm>
#include <utility>

namespace ae {

// =============================================================================
// StreamingResidencyExecutor
// =============================================================================

StreamingResidencyExecutor::StreamingResidencyExecutor()
    : StreamingResidencyExecutor(Config{}) {}

StreamingResidencyExecutor::StreamingResidencyExecutor(Config config)
    : config_(config) {}

StreamingResidencyExecutor::~StreamingResidencyExecutor() {
    // Cancel any in-flight loads.
    if (loader_) {
        for (auto& entry : regions_) {
            if (entry.state == RegionState::Loading && entry.load_handle >= 0) {
                loader_->cancel_load(entry.load_handle);
                entry.load_handle = -1;
            }
        }
    }
}

void StreamingResidencyExecutor::set_loader(std::unique_ptr<IRegionLoader> loader) {
    loader_ = std::move(loader);
}

void StreamingResidencyExecutor::process_transitions(
    const std::vector<RegionTransition>& transitions)
{
    for (const auto& t : transitions) {
        if (t.load) {
            // --- Load transition ---
            RegionEntry* existing = find_entry(t.coord);
            if (existing) {
                switch (existing->state) {
                case RegionState::None:
                case RegionState::Failed:
                case RegionState::Cancelled:
                    // These states can transition to Requested.
                    existing->state = RegionState::Requested;
                    existing->error_message.clear();
                    existing->retry_count = 0;
                    break;
                case RegionState::Requested:
                case RegionState::Loading:
                case RegionState::Resident:
                case RegionState::Evicting:
                    // Already on the right track; nothing to do.
                    break;
                }
            } else {
                // New region — create entry in Requested state.
                RegionEntry& entry = get_or_create_entry(t.coord);
                entry.coord = t.coord;
                entry.state = RegionState::Requested;
            }
        } else {
            // --- Unload transition ---
            RegionEntry* existing = find_entry(t.coord);
            if (!existing) continue;  // Nothing to unload.

            switch (existing->state) {
            case RegionState::Requested:
                // Never started loading; just discard.
                discard_entry(*existing);
                break;
            case RegionState::Loading:
                // Cancel the in-flight load.
                if (existing->load_handle >= 0 && loader_) {
                    loader_->cancel_load(existing->load_handle);
                    --in_flight_count_;
                    existing->load_handle = -1;
                }
                existing->state = RegionState::Cancelled;
                existing->pending_data.reset();
                break;
            case RegionState::Resident:
                // Mark for eviction; data remains readable until eviction completes.
                existing->state = RegionState::Evicting;
                break;
            case RegionState::Failed:
            case RegionState::Cancelled:
            case RegionState::Evicting:
                // Already heading out.
                break;
            case RegionState::None:
                break;
            }
        }
    }
}

void StreamingResidencyExecutor::tick() {
    if (!loader_) return;

    // -----------------------------------------------------------------
    // 1. Drain completion queue
    // -----------------------------------------------------------------
    {
        std::vector<PendingCompletion> batch;
        {
            std::lock_guard<std::mutex> lock(completion_mutex_);
            batch.swap(pending_completions_);
        }
        for (auto& pc : batch) {
            RegionEntry* entry = find_entry(pc.coord);
            if (!entry) continue;

            if (entry->state == RegionState::Loading) {
                if (pc.success) {
                    // Store loaded data as pending (not yet committed).
                    entry->pending_data = std::move(pc.data);
                    // State stays Loading until commit().
                } else {
                    // Load failed.
                    entry->error_message = std::move(pc.error);
                    ++entry->retry_count;
                    if (entry->retry_count < config_.max_retry_count) {
                        // Will be retried below (Requested → Loading).
                        entry->state = RegionState::Requested;
                    } else {
                        entry->state = RegionState::Failed;
                    }
                }
                entry->load_handle = -1;
                --in_flight_count_;
            }
            // If entry is not Loading (e.g. was cancelled), discard completion.
        }
    }

    // -----------------------------------------------------------------
    // 2. Retry failed regions
    // -----------------------------------------------------------------
    for (auto& entry : regions_) {
        if (entry.state == RegionState::Failed &&
            entry.retry_count < config_.max_retry_count)
        {
            entry.state = RegionState::Requested;
        }
    }

    // -----------------------------------------------------------------
    // 3. Process evictions
    // -----------------------------------------------------------------
    for (auto& entry : regions_) {
        if (entry.state == RegionState::Evicting) {
            complete_eviction(entry);
        }
    }

    // -----------------------------------------------------------------
    // 4. Clean up Cancelled entries
    // -----------------------------------------------------------------
    for (auto& entry : regions_) {
        if (entry.state == RegionState::Cancelled) {
            discard_entry(entry);
        }
    }

    // -----------------------------------------------------------------
    // 5. Dispatch new loads (within budget)
    // -----------------------------------------------------------------
    for (auto& entry : regions_) {
        if (entry.state == RegionState::Requested &&
            in_flight_count_ < config_.max_in_flight_loads)
        {
            try_dispatch_load(entry);
            // If dispatch succeeded, in_flight_count_ is incremented and
            // state is now Loading.  Check budget again for next entry.
        }
    }
}

void StreamingResidencyExecutor::commit() {
    std::size_t additional_bytes = 0;
    int commit_count = 0;

    // First pass: compute memory impact.
    for (const auto& entry : regions_) {
        if (entry.state == RegionState::Loading && entry.pending_data) {
            additional_bytes += entry.pending_data->size();
            ++commit_count;
        }
    }

    // Check memory budget.
    const std::size_t new_total = resident_memory_bytes_ + additional_bytes;
    if (new_total > config_.max_memory_bytes) {
        // Over budget — do not commit.  The caller should evict something first.
        return;
    }

    // Second pass: commit.
    for (auto& entry : regions_) {
        if (entry.state == RegionState::Loading && entry.pending_data) {
            entry.data = std::move(entry.pending_data);
            entry.state = RegionState::Resident;
        }
    }

    resident_memory_bytes_ = new_total;
}

// -- Queries ------------------------------------------------------------------

RegionState StreamingResidencyExecutor::state(RegionCoord coord) const {
    for (const auto& entry : regions_) {
        if (entry.coord == coord) return entry.state;
    }
    return RegionState::None;
}

RegionData StreamingResidencyExecutor::data(RegionCoord coord) const {
    for (const auto& entry : regions_) {
        if (entry.coord == coord && entry.state == RegionState::Resident) {
            return entry.data;
        }
    }
    return nullptr;
}

int StreamingResidencyExecutor::resident_count() const {
    int n = 0;
    for (const auto& entry : regions_) {
        if (entry.state == RegionState::Resident) ++n;
    }
    return n;
}

// -- Private helpers ----------------------------------------------------------

StreamingResidencyExecutor::RegionEntry*
StreamingResidencyExecutor::find_entry(RegionCoord coord) {
    for (auto& entry : regions_) {
        if (entry.coord == coord) return &entry;
    }
    return nullptr;
}

StreamingResidencyExecutor::RegionEntry&
StreamingResidencyExecutor::get_or_create_entry(RegionCoord coord) {
    RegionEntry* existing = find_entry(coord);
    if (existing) return *existing;
    return regions_.emplace_back();
}

void StreamingResidencyExecutor::try_dispatch_load(RegionEntry& entry) {
    if (!loader_) return;

    auto on_complete = [this, coord = entry.coord](RegionData loaded_data) {
        PendingCompletion pc;
        pc.coord = coord;
        pc.success = true;
        pc.data = std::move(loaded_data);
        std::lock_guard<std::mutex> lock(completion_mutex_);
        pending_completions_.push_back(std::move(pc));
    };

    auto on_fail = [this, coord = entry.coord](const std::string& error) {
        PendingCompletion pc;
        pc.coord = coord;
        pc.success = false;
        pc.error = error;
        std::lock_guard<std::mutex> lock(completion_mutex_);
        pending_completions_.push_back(std::move(pc));
    };

    const int handle = loader_->start_load(entry.coord, std::move(on_complete), std::move(on_fail));
    entry.load_handle = handle;
    entry.state = RegionState::Loading;
    ++in_flight_count_;
}

void StreamingResidencyExecutor::start_eviction(RegionEntry& entry) {
    entry.state = RegionState::Evicting;
}

void StreamingResidencyExecutor::complete_eviction(RegionEntry& entry) {
    std::size_t freed = 0;
    if (entry.data) {
        freed = entry.data->size();
        entry.data.reset();
    }
    if (resident_memory_bytes_ >= freed) {
        resident_memory_bytes_ -= freed;
    } else {
        resident_memory_bytes_ = 0;
    }
    discard_entry(entry);
}

void StreamingResidencyExecutor::discard_entry(RegionEntry& entry) {
    entry.state = RegionState::None;
    entry.data.reset();
    entry.pending_data.reset();
    entry.error_message.clear();
    entry.load_handle = -1;
    entry.retry_count = 0;
}

}  // namespace ae
