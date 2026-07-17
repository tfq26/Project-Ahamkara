#pragma once

#include "wish/types.h"
#include "wish/log.h"
#include "wish/core/activity.h"
#include "wish/session/session_model.h"

#include <chrono>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace wish::core {

/// Manages multiple concurrent activity instances.
/// Routes packets, ticks activities, and collects snapshots.
class ActivityManager {
public:
    using clock      = std::chrono::steady_clock;
    using time_point = clock::time_point;

    ActivityManager() = default;

    /// Register a template — does not start it yet.
    void register_template(ActivityConfig cfg) {
        templates_.push_back(std::move(cfg));
    }

    /// Start an activity from a registered template by id, or by name.
    ActivityId start_activity(ActivityId template_id,
                              std::unique_ptr<IActivityBase> instance);

    ActivityId start_activity(std::string_view name,
                              std::unique_ptr<IActivityBase> instance);

    /// Route an incoming packet from `from` address.
    /// Returns true if the packet was consumed.
    bool route_packet(const wish::NetAddress& from,
                      std::span<const std::byte> data,
                      time_point now);

    /// Tick every running activity.
    void tick_all(float dt) {
        for (auto& entry : entries_) {
            if (entry.instance && entry.running) {
                entry.instance->tick(dt);
            }
        }
    }

    /// Call fn(sid, data, len) for every snapshot that needs
    /// to be transmitted. The caller sends the data to the client address.
    template <typename Fn>
    void broadcast_snapshots(Fn&& fn) {
        for (auto& entry : entries_) {
            if (!entry.instance || !entry.running) continue;
            entry.instance->for_each_connected_snapshot(
                +[](void* ctx, session::SessionId sid,
                    const std::byte* data, wish::usize len) {
                    auto* f = static_cast<std::remove_reference_t<Fn>*>(ctx);
                    (*f)(sid, data, len);
                },
                &fn);
        }
    }

    /// Map a net address to the session+activity that owns it.
    struct RoutingInfo {
        ActivityId     activity_id {0};
        session::SessionId session_id {0};
    };

    RoutingInfo* find_routing(const wish::NetAddress& address) {
        auto it = routing_.find(address_key(address));
        if (it == routing_.end()) return nullptr;
        return &it->second;
    }

    const RoutingInfo* find_routing(const wish::NetAddress& address) const {
        auto it = routing_.find(address_key(address));
        if (it == routing_.end()) return nullptr;
        return &it->second;
    }

    const ActivityConfig* get_template(ActivityId id) const {
        for (auto& t : templates_) {
            if (t.id == id) return &t;
        }
        return nullptr;
    }

    const ActivityConfig* get_template(std::string_view name) const {
        for (auto& t : templates_) {
            if (t.name == name) return &t;
        }
        return nullptr;
    }

    IActivityBase* get_activity(ActivityId id) {
        for (auto& entry : entries_) {
            if (entry.instance && entry.instance->activity_id() == id) {
                return entry.instance.get();
            }
        }
        return nullptr;
    }

    wish::u32 running_count() const {
        wish::u32 n = 0;
        for (auto& e : entries_) { if (e.running) ++n; }
        return n;
    }

    wish::u32 total_player_count() const {
        wish::u32 n = 0;
        for (auto& e : entries_) {
            if (e.instance && e.running) n += e.instance->player_count();
        }
        return n;
    }

    const auto& templates() const { return templates_; }
    const auto& entries() const { return entries_; }

private:
    struct ActivityEntry {
        std::unique_ptr<IActivityBase> instance;
        bool running {false};
    };

    static std::string address_key(const wish::NetAddress& addr) {
        return addr.ip + ":" + std::to_string(addr.port);
    }

    std::vector<ActivityConfig> templates_;
    std::vector<ActivityEntry> entries_;
    std::unordered_map<std::string, RoutingInfo> routing_;
};

}  // namespace wish::core
