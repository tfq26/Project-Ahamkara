#include "ahamkara/game/adapters/reward_manager.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <random>

namespace ahamkara::game::adapters {

// ===========================================================================
// Default clock
// ===========================================================================

static std::int64_t default_clock_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// ===========================================================================
// RewardManager
// ===========================================================================

RewardManager::RewardManager(RewardManagerConfig config)
    : config_(config)
    , clock_fn_(default_clock_ms) {
}

GrantOutcome RewardManager::grant_reward(
    const ProgressionResult& result,
    const RewardGrantCallback& grant_callback) {
    // 1. Idempotency check
    if (is_already_granted(result.result_id)) {
        return GrantOutcome::kAlreadyGranted;
    }

    // 2. Try to dispatch
    bool dispatched = false;
    try {
        dispatched = grant_callback(result);
    } catch (...) {
        dispatched = false;
    }

    if (dispatched) {
        // Mark as granted
        insert_recent_id(result.result_id);
        return GrantOutcome::kGranted;
    }

    // 3. Backend unavailable — decide based on offline behaviour
    //    (This manager doesn't know the offline policy; it queues always.
    //     The caller/adapter controls rejection at a higher level.)
    if (pending_queue_.size() >= config_.max_offline_queue) {
        // Queue full — reject
        return GrantOutcome::kFailed;
    }

    PendingResult pending{};
    pending.result = result;
    pending.retry_count = 0;
    pending.next_retry_time_ms = clock_fn_() + config_.retry_base_delay_ms;
    pending_queue_.push_back(std::move(pending));

    return GrantOutcome::kPending;
}

std::size_t RewardManager::retry_pending(
    const RewardGrantCallback& grant_callback) {
    if (pending_queue_.empty()) return 0;

    auto now_ms = clock_fn_();
    std::size_t retried = 0;

    std::vector<PendingResult> remaining;
    remaining.reserve(pending_queue_.size());

    for (auto& pending : pending_queue_) {
        if (pending.next_retry_time_ms > now_ms) {
            remaining.push_back(std::move(pending));
            continue;
        }

        // Check idempotency again (in case it was granted by another path)
        if (is_already_granted(pending.result.result_id)) {
            retried++;
            continue;  // skip — already granted
        }

        bool dispatched = false;
        try {
            dispatched = grant_callback(pending.result);
        } catch (...) {
            dispatched = false;
        }

        if (dispatched) {
            insert_recent_id(pending.result.result_id);
            retried++;
            continue;  // success — don't requeue
        }

        // Retry failed — increment or exhaust
        pending.retry_count++;
        if (pending.retry_count >= config_.max_retry_attempts) {
            // Exhausted — drop silently (caller can inspect via snapshot)
            retried++;
            continue;  // drop
        }

        // Exponential backoff with ~25% jitter
        auto delay = config_.retry_base_delay_ms * (1LL << pending.retry_count);
        // Add jitter: ±25%
        std::int64_t jitter = delay / 4;
        // Simple pseudo-random jitter using the heap
        std::int64_t jitter_val = (std::rand() % (jitter * 2 + 1)) - jitter;
        pending.next_retry_time_ms = now_ms + delay + jitter_val;

        remaining.push_back(std::move(pending));
        retried++;
    }

    pending_queue_ = std::move(remaining);
    return retried;
}

bool RewardManager::is_already_granted(const std::string& result_id) const {
    return std::find(recent_result_ids_.begin(), recent_result_ids_.end(),
                     result_id) != recent_result_ids_.end();
}

std::size_t RewardManager::pending_count() const {
    return pending_queue_.size();
}

std::size_t RewardManager::tracked_count() const {
    return recent_result_ids_.size();
}

std::vector<PendingResult> RewardManager::snapshot_pending() const {
    return pending_queue_;
}

void RewardManager::restore_pending(const std::vector<PendingResult>& pending) {
    pending_queue_ = pending;
}

void RewardManager::reset() {
    recent_result_ids_.clear();
    pending_queue_.clear();
}

void RewardManager::set_clock(ClockFn fn) {
    clock_fn_ = fn ? fn : default_clock_ms;
}

bool RewardManager::insert_recent_id(const std::string& result_id) {
    // Bounded LRU-ish set: add to front, evict from back if over limit
    auto it = std::find(recent_result_ids_.begin(), recent_result_ids_.end(),
                        result_id);
    if (it != recent_result_ids_.end()) {
        // Already present — move to front
        std::rotate(recent_result_ids_.begin(), it, it + 1);
        return false;  // was already tracked
    }

    recent_result_ids_.insert(recent_result_ids_.begin(), result_id);
    if (recent_result_ids_.size() > config_.max_recent_ids) {
        recent_result_ids_.resize(config_.max_recent_ids);
    }
    return true;
}

}  // namespace ahamkara::game::adapters
