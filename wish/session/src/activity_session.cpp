#include "wish/session/activity_session.h"

namespace wish::session {

ActivitySession::ActivitySession(wish::u32 id, ActivitySessionConfig config)
    : id_(id)
    , config_(std::move(config))
    , group_(id, config_.max_players) {
}

// ── Client management ──────────────────────────────────────────────────────

ClientSession* ActivitySession::add_client(const wish::NetAddress& addr, time_point now) {
    if (state_ != ActivitySessionState::Lobby) {
        return nullptr;
    }
    return group_.add_client(addr, now);
}

bool ActivitySession::remove_client(const wish::NetAddress& addr) {
    return group_.remove_client(addr);
}

ClientSession* ActivitySession::find_client(const wish::NetAddress& addr) {
    return group_.find_client(addr);
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

bool ActivitySession::start_lobby(time_point now) {
    if (state_ != ActivitySessionState::Idle) {
        return false;
    }
    set_state(ActivitySessionState::Lobby, now);
    return true;
}

bool ActivitySession::launch(time_point now, core::IActivityBase* activity) {
    if (state_ != ActivitySessionState::Lobby) {
        return false;
    }
    if (activity == nullptr) {
        return false;
    }
    activity_ = activity;
    set_state(ActivitySessionState::Active, now);
    return true;
}

bool ActivitySession::complete() {
    if (state_ != ActivitySessionState::Active) {
        return false;
    }
    activity_ = nullptr;
    set_state(ActivitySessionState::Completed, {});
    return true;
}

bool ActivitySession::cancel() {
    if (state_ == ActivitySessionState::Completed || state_ == ActivitySessionState::Cancelled) {
        return false;
    }
    activity_ = nullptr;
    set_state(ActivitySessionState::Cancelled, {});
    return true;
}

void ActivitySession::tick(float dt, time_point now) {
    if (state_ == ActivitySessionState::Active && activity_ != nullptr) {
        activity_->tick(dt);
    }

    // Prune timed-out clients in any state
    group_.tick(now);
}

// ── Timing ─────────────────────────────────────────────────────────────────

ActivitySession::clock::duration ActivitySession::time_in_state(time_point now) const {
    if (state_ == ActivitySessionState::Idle) {
        return clock::duration::zero();
    }
    return now - state_entered_;
}

// ── Internal ───────────────────────────────────────────────────────────────

void ActivitySession::set_state(ActivitySessionState new_state, time_point now) {
    state_ = new_state;
    state_entered_ = now;
}

}  // namespace wish::session
