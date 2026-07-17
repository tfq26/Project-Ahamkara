#pragma once

#include "wish/types.h"
#include "wish/core/activity.h"

#include <memory>
#include <span>
#include <utility>

namespace wish::core {

/// ActivityModifier wraps a base activity and intercepts its methods,
/// enabling composability (e.g. HordeModifier<DeathmatchActivity> = PvEvP).
///
/// Usage:
///   struct HordeModifier : ActivityModifier<DeathmatchActivity> { ... };
///   using PvEvP = HordeModifier;
///
/// The modifier pattern allows stacking: LootModifier<HordeModifier<Base>>.
template <typename BaseActivity>
class ActivityModifier : public IActivityBase {
public:
    explicit ActivityModifier(std::unique_ptr<BaseActivity> base)
        : base_(std::move(base)) {
    }

    bool initialize(const ActivityConfig& cfg) override {
        return base_->initialize(cfg);
    }

    void shutdown() override {
        base_->shutdown();
    }

    bool admit_player(const SessionAdmissionRequest& req) override {
        return base_->admit_player(req);
    }

    void remove_player(session::SessionId sid) override {
        base_->remove_player(sid);
    }

    wish::u32 player_count() const override {
        return base_->player_count();
    }

    void tick(float dt) override {
        base_->tick(dt);
    }

    void process_input(session::SessionId sid,
                       const wish::PacketEnvelope& envelope,
                       wish::u32 command_sequence) override {
        base_->process_input(sid, envelope, command_sequence);
    }

    wish::usize build_snapshot_bytes(session::SessionId sid,
                                     std::span<std::byte> buffer) override {
        return base_->build_snapshot_bytes(sid, buffer);
    }

    bool is_complete() const override {
        return base_->is_complete();
    }

    ActivityId activity_id() const override {
        return base_->activity_id();
    }

    ActivityCategory category() const override {
        return base_->category();
    }

    std::string_view activity_name() const override {
        return base_->activity_name();
    }

    void for_each_connected_snapshot(
        void (*fn)(void* ctx, session::SessionId sid,
                   const std::byte* data, wish::usize len),
        void* ctx) override {
        base_->for_each_connected_snapshot(fn, ctx);
    }

protected:
    BaseActivity& base() { return *base_; }
    const BaseActivity& base() const { return *base_; }

private:
    std::unique_ptr<BaseActivity> base_;
};

}  // namespace wish::core
