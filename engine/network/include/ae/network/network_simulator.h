#pragma once

#include "ae/core/types.h"
#include "ae/network/udp_socket.h"

#include <chrono>
#include <deque>
#include <functional>
#include <random>
#include <vector>

namespace ae {

/**
 * @brief Configurable packet-loss and artificial-latency simulation layer.
 *
 * Wraps an existing UdpSocket and can be placed between application logic and
 * the real wire to inject controlled degradation.  All delay/packet-order
 * semantics are strictly configurable; the simulator does not mutate packet
 * contents.
 *
 * The intended use is togglable debug tooling during netcode development,
 * not a production runtime path.
 */
struct SimulatorConfig {
    /** Packet loss probability [0.0, 1.0].  0.0 = no loss, 1.0 = all dropped. */
    float loss_rate {0.0F};

    /** Minimum round-trip latency in milliseconds (one-way delay is half). */
    float latency_min_ms {0.0F};

    /** Maximum round-trip latency in milliseconds (one-way delay is half). */
    float latency_max_ms {0.0F};

    /** Standard deviation of jitter applied to each packet's delay (ms). */
    float jitter_ms {0.0F};

    /** If true, the simulator is completely bypassed (wire passthrough). */
    bool enabled {false};
};

/**
 * @brief Statistics gathered while the simulator is enabled.
 */
struct SimulatorStats {
    u64 packets_received {0};
    u64 packets_dropped {0};
    u64 packets_delayed {0};
    u64 packets_sent {0};
    u64 packets_expired {0};   ///< Delayed packets whose TTL elapsed.
    u64 bytes_received {0};
    u64 bytes_sent {0};

    /** Resets all counters to zero. */
    void reset() {
        *this = SimulatorStats{};
    }
};

/**
 * @brief Debug-oriented network degradation simulator.
 *
 * Insertion point:
 * @code
 *   UdpSocket real_socket;
 *   NetworkSimulator sim(real_socket);
 *   sim.send_to(...);        // may drop or delay
 *   sim.receive_from(...);   // may drop incoming, deliver delayed packets
 * @endcode
 */
class NetworkSimulator {
public:
    explicit NetworkSimulator(UdpSocket& wrapped_socket);

    NetworkSimulator(const NetworkSimulator&) = delete;
    NetworkSimulator& operator=(const NetworkSimulator&) = delete;

    /**
     * @brief Apply the given configuration.
     *
     * Setting `enabled = false` bypasses all simulation logic.  Any packets
     * currently held in the delay queue are flushed immediately.
     */
    void configure(const SimulatorConfig& config);

    [[nodiscard]] const SimulatorConfig& config() const { return config_; }
    [[nodiscard]] const SimulatorStats& stats() const { return stats_; }

    /** Reset statistics counters. */
    void reset_stats();

    // ── Forwarded socket operations ────────────────────────────────────────

    /**
     * @brief Send a packet with optional loss/delay simulation.
     *
     * When enabled and the packet is not dropped, it is placed in the
     * delay queue with a computed hold time.  When disabled, forwards
     * directly to the wrapped socket.
     */
    bool send_to(const NetAddress& address, const void* data, usize size);

    /**
     * @brief Receive packets, including delayed ones whose hold time has
     *        expired.  Packets that have been in the queue longer than their
     *        configured TTL are dropped (counted as expired).
     *
     * When the simulator is disabled, forwards directly to the wrapped socket.
     */
    i32 receive_from(NetAddress& from, void* buffer, usize max_size);

    /**
     * @brief Call every frame with the elapsed wall-clock time since the
     *        last call (in seconds).  Processes the delay queue and flushes
     *        packets whose hold time has expired.
     */
    void update(float delta_seconds);

    // ── Access to wrapped socket ───────────────────────────────────────────

    [[nodiscard]] UdpSocket& socket() { return socket_; }
    [[nodiscard]] const UdpSocket& socket() const { return socket_; }

    [[nodiscard]] bool is_open() const { return socket_.is_open(); }

private:
    struct DelayedPacket {
        std::vector<std::byte> data;
        NetAddress address;
        float remaining_seconds {0.0F};
    };

    bool should_drop();
    float compute_delay_seconds();

    UdpSocket& socket_;
    SimulatorConfig config_;
    SimulatorStats stats_;

    std::deque<DelayedPacket> delay_queue_;

    // Thread-local RNG to avoid contention in multi-client test harnesses.
    std::mt19937 rng_;
    std::uniform_real_distribution<float> dist_;
};

}  // namespace ae
