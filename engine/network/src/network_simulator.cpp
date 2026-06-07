#include "ae/network/network_simulator.h"

#include "ae/core/log.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <sstream>
#include <vector>

namespace ae {

NetworkSimulator::NetworkSimulator(UdpSocket& wrapped_socket)
    : socket_(wrapped_socket)
    , rng_(std::random_device{}())
    , dist_(0.0F, 1.0F) {
}

void NetworkSimulator::configure(const SimulatorConfig& config) {
    config_ = config;

    if (!config_.enabled) {
        // Flush any queued delayed packets immediately.
        if (!delay_queue_.empty()) {
            u64 flushed = 0;
            for (const auto& pkt : delay_queue_) {
                if (socket_.send_to(pkt.address, pkt.data.data(), pkt.data.size())) {
                    ++stats_.packets_sent;
                    stats_.bytes_sent += pkt.data.size();
                }
                ++flushed;
            }
            delay_queue_.clear();

            std::ostringstream msg;
            msg << "NetworkSimulator disabled: flushed " << flushed << " delayed packets.";
            log_info(msg.str());
        }
    }

    {
        std::ostringstream msg;
        msg << "NetworkSimulator configured: enabled=" << (config_.enabled ? "yes" : "no")
            << " loss=" << (config_.loss_rate * 100.0F) << "%"
            << " latency=[" << config_.latency_min_ms << ", " << config_.latency_max_ms << "]ms"
            << " jitter=" << config_.jitter_ms << "ms";
        log_info(msg.str());
    }
}

void NetworkSimulator::reset_stats() {
    stats_.reset();
}

// ── Internal helpers ───────────────────────────────────────────────────────────

bool NetworkSimulator::should_drop() {
    if (config_.loss_rate <= 0.0F) {
        return false;
    }
    if (config_.loss_rate >= 1.0F) {
        return true;
    }
    return dist_(rng_) < config_.loss_rate;
}

float NetworkSimulator::compute_delay_seconds() {
    const float range = config_.latency_max_ms - config_.latency_min_ms;
    const float base_ms = config_.latency_min_ms + dist_(rng_) * range;

    // Apply Gaussian jitter centred on the chosen delay.
    float jitter = 0.0F;
    if (config_.jitter_ms > 0.0F) {
        std::normal_distribution<float> jitter_dist(0.0F, config_.jitter_ms);
        jitter = jitter_dist(rng_);
    }

    const float one_way_ms = (base_ms + jitter) * 0.5F;
    return std::max(0.0F, one_way_ms) / 1000.0F;
}

// ── Public interface ───────────────────────────────────────────────────────────

bool NetworkSimulator::send_to(const NetAddress& address, const void* data, usize size) {
    if (!config_.enabled) {
        const bool ok = socket_.send_to(address, data, size);
        if (ok) {
            ++stats_.packets_sent;
            stats_.bytes_sent += size;
        }
        return ok;
    }

    ++stats_.packets_received;

    if (should_drop()) {
        ++stats_.packets_dropped;
        return true;  // The caller sees "success" to avoid changing application flow.
    }

    // Queue the packet for delayed delivery.
    DelayedPacket pkt;
    pkt.address = address;
    pkt.data.assign(static_cast<const std::byte*>(data),
                    static_cast<const std::byte*>(data) + size);
    pkt.remaining_seconds = compute_delay_seconds();

    ++stats_.packets_delayed;
    delay_queue_.push_back(std::move(pkt));
    return true;
}

i32 NetworkSimulator::receive_from(NetAddress& from, void* buffer, usize max_size) {
    // First drain real socket (incoming packets are NOT delayed on receive
    // side — the server injects delay on send; this keeps the model simple).
    const i32 received = socket_.receive_from(from, buffer, max_size);

    if (!config_.enabled) {
        if (received > 0) {
            ++stats_.packets_sent;
            stats_.bytes_sent += static_cast<usize>(received);
        }
        return received;
    }

    if (received <= 0) {
        return received;
    }

    ++stats_.packets_received;
    stats_.bytes_received += static_cast<usize>(received);

    // Apply loss on incoming side too (simulates asymmetric loss).
    if (should_drop()) {
        ++stats_.packets_dropped;
        // Signal "no data available" to caller.
        return 0;
    }

    return received;
}

void NetworkSimulator::update(float delta_seconds) {
    if (!config_.enabled) {
        return;
    }

    // Flush delayed packets whose hold time has expired.
    while (!delay_queue_.empty()) {
        auto& pkt = delay_queue_.front();
        pkt.remaining_seconds -= delta_seconds;

        if (pkt.remaining_seconds > 0.0F) {
            break;  // Still waiting on this and later packets.
        }

        // Packet ready to send.
        if (socket_.send_to(pkt.address, pkt.data.data(), pkt.data.size())) {
            ++stats_.packets_sent;
            stats_.bytes_sent += pkt.data.size();
        }
        delay_queue_.pop_front();
    }

    // Drop packets that have been in queue too long (TTL = 5 seconds).
    // This prevents unbounded queue growth.
    if (!delay_queue_.empty()) {
        auto it = delay_queue_.begin();
        while (it != delay_queue_.end()) {
            if (it->remaining_seconds < -5.0F) {
                ++stats_.packets_expired;
                it = delay_queue_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

}  // namespace ae
