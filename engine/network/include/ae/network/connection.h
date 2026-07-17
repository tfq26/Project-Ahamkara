#pragma once

#include "ae/core/types.h"
#include "ae/network/packet_envelope.h"
#include "ae/network/sequence_tracker.h"
#include "ae/network/udp_socket.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

namespace ae {

/**
 * @brief Explicit connection lifecycle states.
 *
 * Transitions:
 *   Disconnected ──[connect_request]──> Handshaking
 *   Handshaking   ──[handshake_ok]────> Connected
 *   Connected     ──[disconnect]──────> Disconnecting
 *   Connected     ──[timeout]─────────> GracePeriod
 *   GracePeriod   ──[reconnect]───────> Connected
 *   GracePeriod   ──[grace_expired]───> Disconnected
 *   Disconnecting ──[cleanup]─────────> Disconnected
 */
enum class ConnectionState : u8 {
    Disconnected,
    Handshaking,
    Connected,
    GracePeriod,
    Disconnecting
};

/// Human-readable name for a connection state.
inline const char* connection_state_name(ConnectionState s) {
    switch (s) {
        case ConnectionState::Disconnected:  return "Disconnected";
        case ConnectionState::Handshaking:   return "Handshaking";
        case ConnectionState::Connected:     return "Connected";
        case ConnectionState::GracePeriod:   return "GracePeriod";
        case ConnectionState::Disconnecting: return "Disconnecting";
    }
    return "Unknown";
}

/**
 * @brief Per-peer connection record.
 *
 * Each connected or connecting peer has one of these tracked by the
 * ConnectionManager.  It holds the address, state machine state,
 * timing metadata, and the sequence+reliable-channel bookkeeping.
 */
struct PeerConnection {
    NetAddress address {};
    ConnectionState state {ConnectionState::Disconnected};

    /// Monotonic wall-clock time of the last packet from this peer.
    std::chrono::steady_clock::time_point last_seen {};

    /// Monotonic wall-clock time when the peer entered its current state.
    std::chrono::steady_clock::time_point state_entered {};

    /// Opaque session identifier assigned during handshake.
    u64 session_id {0};

    /// Sequence tracking for incoming/outgoing packets.
    SequenceTracker sequence_tracker {};

    /// Number of missed heartbeats (incremented on each heartbeat timeout).
    u32 missed_heartbeats {0};

    /// If the peer is in GracePeriod, the original session state is
    /// preserved so it can resume on reconnect.
    bool preserves_session {false};
};

/**
 * @brief Manages the lifecycle of all peer connections.
 *
 * Provides the authoritative state machine transitions for connection
 * lifecycle.  Designed to replace the ad-hoc PeerState / handshake_list
 * in dedicated_server_main.cpp.
 *
 * All timing is driven by the caller via tick() — this class does NOT
 * spawn its own threads.
 */
class ConnectionManager {
public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    explicit ConnectionManager(
        clock::duration handshake_timeout = std::chrono::seconds(5),
        clock::duration disconnect_timeout = std::chrono::seconds(10),
        clock::duration grace_period = std::chrono::seconds(30),
        u32 max_missed_heartbeats = 3)
        : handshake_timeout_(handshake_timeout)
        , disconnect_timeout_(disconnect_timeout)
        , grace_period_(grace_period)
        , max_missed_heartbeats_(max_missed_heartbeats) {}

    // ── Lookup ──────────────────────────────────────────────────────────

    /// Find a peer by address. Returns nullptr if not found.
    [[nodiscard]] PeerConnection* find(const NetAddress& address) {
        auto it = peers_.find(address_key(address));
        return (it == peers_.end()) ? nullptr : &it->second;
    }

    [[nodiscard]] const PeerConnection* find(const NetAddress& address) const {
        auto it = peers_.find(address_key(address));
        return (it == peers_.end()) ? nullptr : &it->second;
    }

    [[nodiscard]] PeerConnection* find_by_session(u64 session_id) {
        for (auto& [key, peer] : peers_) {
            (void)key;
            if (peer.session_id == session_id) {
                return &peer;
            }
        }
        return nullptr;
    }

    // ── State transitions ───────────────────────────────────────────────

    /**
     * @brief Register or touch a peer.
     *
     * If the peer does not exist, it is created in Handshaking state.
     * If it exists in Disconnected or GracePeriod, it transitions to
     * Handshaking.  Otherwise just updates last_seen.
     */
    PeerConnection& connect_request(const NetAddress& address, time_point now) {
        auto* existing = find(address);
        if (existing) {
            if (existing->state == ConnectionState::GracePeriod) {
                // Graceful reconnect — preserve session state
                existing->state = ConnectionState::Connected;
                existing->last_seen = now;
                existing->state_entered = now;
                existing->missed_heartbeats = 0;
                existing->preserves_session = true;
                return *existing;
            }
            if (existing->state == ConnectionState::Disconnected) {
                existing->state = ConnectionState::Handshaking;
                existing->last_seen = now;
                existing->state_entered = now;
                existing->missed_heartbeats = 0;
                return *existing;
            }
            // Already in an active state — just touch.
            existing->last_seen = now;
            return *existing;
        }

        // New peer.
        PeerConnection peer;
        peer.address = address;
        peer.state = ConnectionState::Handshaking;
        peer.last_seen = now;
        peer.state_entered = now;
        peers_[address_key(address)] = std::move(peer);
        return peers_[address_key(address)];
    }

    /**
     * @brief Complete a handshake — move from Handshaking to Connected.
     */
    bool complete_handshake(const NetAddress& address, u64 session_id, time_point now) {
        auto* peer = find(address);
        if (!peer) return false;
        if (peer->state != ConnectionState::Handshaking) return false;

        peer->state = ConnectionState::Connected;
        peer->session_id = session_id;
        peer->last_seen = now;
        peer->state_entered = now;
        return true;
    }

    /**
     * @brief Record activity from a peer (touch last_seen, clear heartbeat counter).
     */
    void touch(const NetAddress& address, time_point now) {
        auto* peer = find(address);
        if (!peer) return;
        peer->last_seen = now;

        // If in an active state, clear missed heartbeats.
        if (peer->state == ConnectionState::Connected ||
            peer->state == ConnectionState::Handshaking) {
            peer->missed_heartbeats = 0;
        }
    }

    /**
     * @brief Initiate a disconnect.  Moves to Disconnecting.
     */
    bool disconnect(const NetAddress& address) {
        auto* peer = find(address);
        if (!peer) return false;
        if (peer->state != ConnectionState::Connected &&
            peer->state != ConnectionState::GracePeriod) {
            return false;
        }
        peer->state = ConnectionState::Disconnecting;
        peer->state_entered = clock::now();
        return true;
    }

    /**
     * @brief Remove a peer entirely.
     */
    bool remove(const NetAddress& address) {
        return peers_.erase(address_key(address)) > 0;
    }

    // ── Tick / pruning ──────────────────────────────────────────────────

    /**
     * @brief Advance the state machine.  Called once per server tick.
     *
     * - Handshaking peers past handshake_timeout → Disconnected (removed)
     * - Connected peers with missed_heartbeats > max → GracePeriod
     * - GracePeriod peers past grace_period → Disconnected (removed)
     * - Disconnecting peers are immediately removed
     */
    void tick(time_point now) {
        std::vector<std::string> to_remove;

        for (auto& [key, peer] : peers_) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(
                now - peer.last_seen).count();

            switch (peer.state) {
            case ConnectionState::Handshaking:
                if (elapsed > std::chrono::duration<float>(handshake_timeout_).count()) {
                    to_remove.push_back(key);
                }
                break;

            case ConnectionState::Connected:
                // If we haven't heard from the peer, increment missed heartbeats.
                // This is checked externally by the caller calling mark_missed_heartbeat().
                if (peer.missed_heartbeats > max_missed_heartbeats_) {
                    peer.state = ConnectionState::GracePeriod;
                    peer.state_entered = now;
                }
                break;

            case ConnectionState::GracePeriod:
                if (elapsed > std::chrono::duration<float>(grace_period_).count()) {
                    to_remove.push_back(key);
                }
                break;

            case ConnectionState::Disconnecting:
                to_remove.push_back(key);
                break;

            default:
                break;
            }
        }

        for (const auto& key : to_remove) {
            peers_.erase(key);
        }
    }

    /**
     * @brief Mark a heartbeat as missed for the given peer.
     */
    void mark_missed_heartbeat(const NetAddress& address) {
        auto* peer = find(address);
        if (peer) {
            ++peer->missed_heartbeats;
        }
    }

    // ── Queries ─────────────────────────────────────────────────────────

    [[nodiscard]] std::size_t count() const { return peers_.size(); }

    [[nodiscard]] std::size_t count_by_state(ConnectionState state) const {
        return static_cast<std::size_t>(std::count_if(
            peers_.begin(), peers_.end(),
            [state](const auto& entry) { return entry.second.state == state; }));
    }

    [[nodiscard]] std::size_t connected_count() const {
        return count_by_state(ConnectionState::Connected);
    }

    // ── Iteration ───────────────────────────────────────────────────────

    template <typename Fn>
    void for_each(Fn&& fn) {
        for (auto& [key, peer] : peers_) {
            (void)key;
            fn(peer);
        }
    }

    template <typename Fn>
    void for_each(Fn&& fn) const {
        for (const auto& [key, peer] : peers_) {
            (void)key;
            fn(peer);
        }
    }

    template <typename Fn>
    void for_each_state(ConnectionState state, Fn&& fn) {
        for (auto& [key, peer] : peers_) {
            (void)key;
            if (peer.state == state) {
                fn(peer);
            }
        }
    }

    // ── Config ──────────────────────────────────────────────────────────

    void set_handshake_timeout(clock::duration d) { handshake_timeout_ = d; }
    void set_disconnect_timeout(clock::duration d) { disconnect_timeout_ = d; }
    void set_grace_period(clock::duration d) { grace_period_ = d; }
    void set_max_missed_heartbeats(u32 n) { max_missed_heartbeats_ = n; }

    [[nodiscard]] clock::duration handshake_timeout() const { return handshake_timeout_; }
    [[nodiscard]] clock::duration disconnect_timeout() const { return disconnect_timeout_; }
    [[nodiscard]] clock::duration grace_period() const { return grace_period_; }
    [[nodiscard]] u32 max_missed_heartbeats() const { return max_missed_heartbeats_; }

    /// Remove all peers.
    void clear() { peers_.clear(); }

private:
    static std::string address_key(const NetAddress& addr) {
        return addr.ip + ":" + std::to_string(addr.port);
    }

    std::unordered_map<std::string, PeerConnection> peers_;

    clock::duration handshake_timeout_;
    clock::duration disconnect_timeout_;
    clock::duration grace_period_;
    u32 max_missed_heartbeats_;
};

}  // namespace ae
