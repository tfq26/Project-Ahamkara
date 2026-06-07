#include "wish/session/session_runtime.h"

#include <cassert>
#include <chrono>

namespace {

void test_tracks_multiple_clients() {
    wish::session::SessionRuntime runtime(std::chrono::seconds(2));
    const auto base_time = wish::session::SessionRuntime::clock::time_point {};

    const ae::NetAddress client_a {"127.0.0.1", 20001};
    const ae::NetAddress client_b {"127.0.0.1", 20002};

    auto& session_a = runtime.touch_client(client_a, base_time);
    auto& session_b = runtime.touch_client(client_b, base_time + std::chrono::milliseconds(100));

    assert(runtime.client_count() == 2);
    assert(runtime.connected_client_count() == 0);
    assert(session_a.identity == "127.0.0.1:20001");
    assert(session_b.identity == "127.0.0.1:20002");
    assert(session_a.connection_state == wish::session::ClientConnectionState::PendingAdmission);
    assert(session_b.connection_state == wish::session::ClientConnectionState::PendingAdmission);
}

void test_input_processing_and_timeout_cleanup() {
    wish::session::SessionRuntime runtime(std::chrono::seconds(1));
    const auto base_time = wish::session::SessionRuntime::clock::time_point {};

    const ae::NetAddress client_a {"127.0.0.1", 21001};
    const ae::NetAddress client_b {"127.0.0.1", 21002};

    ae::PacketEnvelope envelope_a {};
    envelope_a.sequence = 11;
    ahamkara::game::PlayerInputCommand command_a {};
    command_a.sequence = 41;

    ae::PacketEnvelope envelope_b {};
    envelope_b.sequence = 12;
    ahamkara::game::PlayerInputCommand command_b {};
    command_b.sequence = 42;

    auto& session_a = runtime.record_input(client_a, envelope_a, command_a, base_time);
    runtime.mark_input_processed(session_a, command_a.sequence);
    auto& session_b = runtime.record_input(client_b, envelope_b, command_b, base_time + std::chrono::milliseconds(600));
    runtime.mark_input_processed(session_b, command_b.sequence);

    assert(runtime.connected_client_count() == 2);
    assert(session_a.connection_state == wish::session::ClientConnectionState::Connected);
    assert(session_b.connection_state == wish::session::ClientConnectionState::Connected);
    assert(session_a.last_processed_input_sequence == 41);
    assert(session_b.last_processed_input_sequence == 42);
    assert(session_a.sequence_tracker.packets_received() == 1);
    assert(session_b.sequence_tracker.packets_received() == 1);

    runtime.prune_timed_out_clients(base_time + std::chrono::milliseconds(1200));

    assert(runtime.client_count() == 1);
    assert(runtime.connected_client_count() == 1);
    const auto* remaining = runtime.find_client(client_b);
    assert(remaining != nullptr);
    assert(remaining->identity == "127.0.0.1:21002");
}

}  // namespace

int main() {
    test_tracks_multiple_clients();
    test_input_processing_and_timeout_cleanup();
    return 0;
}

