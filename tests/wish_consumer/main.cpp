#include "wish/core/engine_identity.h"
#include "wish/core/session_services.h"
#include "wish/net/transport_config.h"
#include "wish/replication/replication_frame.h"
#include "wish/session/session_model.h"
#include "wish/types.h"
#include "wish/log.h"

#include <cassert>
#include <iostream>
#include <string_view>

namespace {

void test_types_standalone() {
    // Verify wish type aliases compile and have expected sizes
    static_assert(sizeof(wish::u8) == 1, "wish::u8 must be 1 byte");
    static_assert(sizeof(wish::u16) == 2, "wish::u16 must be 2 bytes");
    static_assert(sizeof(wish::u32) == 4, "wish::u32 must be 4 bytes");
    static_assert(sizeof(wish::u64) == 8, "wish::u64 must be 8 bytes");

    // Verify NetAddress
    wish::NetAddress addr1 {"127.0.0.1", 7777};
    assert(addr1.ip == "127.0.0.1");
    assert(addr1.port == 7777);

    wish::NetAddress addr2 {"127.0.0.1", 7777};
    wish::NetAddress addr3 {"127.0.0.1", 7778};
    assert(addr1 == addr2);
    assert(addr1 != addr3);

    // Verify PacketEnvelope
    wish::PacketEnvelope env {};
    assert(env.sequence == 0);
    assert(env.ack_sequence == 0);
    assert(env.ack_bitfield == 0);

    // Verify trim
    assert(wish::trim("  hello  ") == "hello");
    assert(wish::trim("\t\nhello\r\n") == "hello");
    assert(wish::trim("") == "");
    assert(wish::trim("   ") == "");

    // Verify SequenceTracker compiles and basic operations work
    wish::SequenceTracker tracker {};
    assert(tracker.packets_sent() == 0);
    assert(tracker.packets_received() == 0);

    wish::PacketEnvelope out = tracker.prepare_outgoing();
    assert(out.sequence == 0);
    assert(tracker.packets_sent() == 1);

    wish::PacketEnvelope env_in {};
    env_in.sequence = 0;
    tracker.process_incoming(env_in);
    assert(tracker.packets_received() == 1);

    std::cout << "test_types_standalone: ok\n";
}

void test_log_standalone() {
    // Verify log functions compile and link
    wish::log_info("test_log_standalone: info message");
    wish::log_warning("test_log_standalone: warning message");
    wish::log_error("test_log_standalone: error message");
    std::cout << "test_log_standalone: compiled and linked ok\n";
}

void test_engine_identity() {
    const auto& id = wish::core::identity();
    assert(!id.name.empty());
    assert(!id.version.empty());
    std::cout << "test_engine_identity: name='" << id.name
              << "' version='" << id.version << "'\n";
}

void test_session_model_is_neutral() {
    wish::session::SessionModel model {};
    assert(model.id.value == 0);
    assert(!model.connected);
    assert(model.server_tick == 0);
    std::cout << "test_session_model_is_neutral: ok\n";
}

void test_replication_frame_is_template() {
    wish::replication::ReplicationFrame<int> frame {};
    assert(frame.server_tick == 0);
    assert(frame.last_processed_input == 0);
    assert(frame.authoritative);
    assert(frame.snapshot == 0);
    std::cout << "test_replication_frame_is_template: ok\n";
}

void test_transport_config() {
    wish::net::TransportConfig config {};
    assert(config.port == 0);
    assert(config.max_packet_size == 1200);
    std::cout << "test_transport_config: ok\n";
}

void test_session_services_are_pure_wish() {
    wish::core::AuthRequest req {"test-token", "127.0.0.1:7777"};
    assert(!req.token.empty());
    assert(!req.remote_endpoint.empty());

    wish::core::SessionAdmissionRequest adm {};
    assert(adm.player_id.empty());

    wish::core::MatchResult match {};
    assert(!match.completed);

    std::cout << "test_session_services_are_pure_wish: ok\n";
}

} // namespace

int main() {
    test_types_standalone();
    test_log_standalone();
    test_engine_identity();
    test_session_model_is_neutral();
    test_replication_frame_is_template();
    test_transport_config();
    test_session_services_are_pure_wish();
    std::cout << "All wish_consumer tests passed.\n";
    return 0;
}
