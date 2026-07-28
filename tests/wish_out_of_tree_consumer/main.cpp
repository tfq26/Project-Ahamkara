// Out-of-tree Wish SDK consumer smoke test.
//
// This is the "dummy external consumer" that proves the installed Wish SDK
// boundary works.  It uses only find_package(Wish) and Wish::* targets.
// No Ahamkara or Flashback headers are referenced.
//
// Build (from a clean prefix):
//   cmake -S tests/wish_out_of_tree_consumer -B build/wish-consumer \
//         -DCMAKE_PREFIX_PATH=/path/to/wish-install
//   cmake --build build/wish-consumer
//   ./build/wish-consumer/wish_consumer

#include "wish/types.h"
#include "wish/log.h"
#include "wish/core/engine_identity.h"
#include "wish/core/session_services.h"
#include "wish/core/error_codes.h"
#include "wish/core/error_envelope.h"
#include "wish/core/error_catalog.h"
#include "wish/net/transport_config.h"
#include "wish/replication/replication_frame.h"
#include "wish/session/session_model.h"
#include "wish/session/session_group.h"
#include "wish/admin/admin_command.h"
#include "wish/admin/heartbeat_service.h"
#include "wish/admin/metrics_collector.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <string_view>

namespace {

// ===================================================================
// 1. Core type aliases
// ===================================================================
void test_types_standalone() {
    static_assert(sizeof(wish::u8) == 1, "wish::u8 must be 1 byte");
    static_assert(sizeof(wish::u16) == 2, "wish::u16 must be 2 bytes");
    static_assert(sizeof(wish::u32) == 4, "wish::u32 must be 4 bytes");
    static_assert(sizeof(wish::u64) == 8, "wish::u64 must be 8 bytes");

    // NetAddress
    wish::NetAddress addr1 {"127.0.0.1", 7777};
    assert(addr1.ip == "127.0.0.1");
    assert(addr1.port == 7777);

    wish::NetAddress addr2 {"127.0.0.1", 7777};
    wish::NetAddress addr3 {"127.0.0.1", 7778};
    assert(addr1 == addr2);
    assert(addr1 != addr3);

    // PacketEnvelope
    wish::PacketEnvelope env {};
    assert(env.sequence == 0);
    assert(env.ack_sequence == 0);
    assert(env.ack_bitfield == 0);

    // trim
    assert(wish::trim("  hello  ") == "hello");
    assert(wish::trim("\t\nhello\r\n") == "hello");
    assert(wish::trim("") == "");
    assert(wish::trim("   ") == "");

    std::cout << "  types_standalone: ok\n";
}

// ===================================================================
// 2. SequenceTracker
// ===================================================================
void test_sequence_tracker() {
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

    std::cout << "  sequence_tracker: ok\n";
}

// ===================================================================
// 3. Logging
// ===================================================================
void test_log() {
    // Verify log functions compile and link without Ahamkara deps
    wish::log_info("test_log: info message");
    wish::log_warning("test_log: warning message");
    wish::log_error("test_log: error message");
    std::cout << "  log: compiled and linked ok\n";
}

// ===================================================================
// 4. Engine identity
// ===================================================================
void test_engine_identity() {
    const auto& id = wish::core::identity();
    assert(!id.name.empty());
    assert(!id.version.empty());
    std::cout << "  engine_identity: name='" << id.name
              << "' version='" << id.version << "'\n";
}

// ===================================================================
// 5. Session model (game-neutral)
// ===================================================================
void test_session_model() {
    wish::session::SessionModel model {};
    assert(model.id.value == 0);
    assert(!model.connected);
    assert(model.server_tick == 0);
    std::cout << "  session_model: ok\n";
}

// ===================================================================
// 6. Session group
// ===================================================================
void test_session_group() {
    wish::session::SessionGroup group {1, 8};
    assert(group.group_id() == 1);
    assert(group.max_clients() == 8);
    assert(group.client_count() == 0);
    assert(group.connected_count() == 0);
    assert(!group.is_full());
    std::cout << "  session_group: ok\n";
}

// ===================================================================
// 7. Replication frame (template, game-neutral)
// ===================================================================
void test_replication_frame() {
    wish::replication::ReplicationFrame<int> frame {};
    assert(frame.server_tick == 0);
    assert(frame.last_processed_input == 0);
    assert(frame.authoritative);
    assert(frame.snapshot == 0);
    std::cout << "  replication_frame: ok\n";
}

// ===================================================================
// 8. Transport config
// ===================================================================
void test_transport_config() {
    wish::net::TransportConfig config {};
    assert(config.port == 0);
    assert(config.max_packet_size == 1200);
    std::cout << "  transport_config: ok\n";
}

// ===================================================================
// 9. Session services (game-neutral)
// ===================================================================
void test_session_services() {
    wish::core::AuthRequest req {"test-token", "127.0.0.1:7777"};
    assert(!req.token.empty());
    assert(!req.remote_endpoint.empty());

    wish::core::SessionAdmissionRequest adm {};
    assert(adm.player_id.empty());

    wish::core::MatchResult match {};
    assert(!match.completed);

    // AuthValidator interface compiles and links
    class TestAuth final : public wish::core::AuthValidator {
    public:
        wish::core::AuthResult validate(const wish::core::AuthRequest&) const override {
            return wish::core::AuthResult{};
        }
    };

    std::cout << "  session_services: ok\n";
}

// ===================================================================
// 10. Error codes and envelope (versioned, stable diagnostics)
// ===================================================================
void test_error_envelope() {
    // Verify versioned envelope
    assert(wish::kErrorEnvelopeVersion == 1);

    // Create a valid envelope
    wish::ErrorEnvelope env;
    env.error_code = static_cast<std::uint32_t>(wish::WishErrorCode::kAuthRejected);
    env.incident_id = "A1B2C3D4";
    env.message_key = "errors.auth.rejected";
    assert(env.valid());
    assert(env.has_error());

    // Serialize and deserialize round-trip
    auto serialized = wish::serialize_envelope(env);
    assert(!serialized.empty());

    auto deserialized = wish::deserialize_envelope(serialized);
    assert(deserialized.has_value());
    assert(deserialized->error_code == static_cast<std::uint32_t>(wish::WishErrorCode::kAuthRejected));
    assert(deserialized->incident_id == "A1B2C3D4");
    assert(deserialized->message_key == "errors.auth.rejected");

    std::cout << "  error_envelope: ok\n";
}

// ===================================================================
// 11. Error catalog (stable diagnostic identity)
// ===================================================================
void test_error_catalog() {
    const auto& catalog = wish::ErrorCatalog::instance();
    assert(catalog.size() >= 10);

    const auto* entry = catalog.find(wish::WishErrorCode::kAuthRejected);
    assert(entry != nullptr);
    assert(entry->code == wish::WishErrorCode::kAuthRejected);
    assert(std::strcmp(entry->domain, "AUT") == 0);
    assert(std::strcmp(entry->message_key, "errors.auth.rejected") == 0);

    // WS-* code formatting
    char buffer[12];
    wish::format_wish_code(wish::WishErrorCode::kCapacityExceeded, buffer, sizeof(buffer));
    assert(std::strcmp(buffer, "WS-CAP-3001") == 0);

    // Verify registered codes produce valid WS-* strings
    const std::uint32_t known_codes[] = {1001, 1002, 2001, 2002, 3001, 4001, 4002, 5001, 5002, 9001};
    for (const auto code_val : known_codes) {
        const auto* entry = catalog.find(code_val);
        assert(entry != nullptr);
        wish::format_wish_code(entry->code, buffer, sizeof(buffer));
        assert(buffer[0] == 'W');
        assert(buffer[1] == 'S');
        assert(buffer[2] == '-');
        assert(buffer[3] != '\0');
    }

    // Versioned envelope format
    assert(wish::kErrorEnvelopeVersion == 1);

    std::cout << "  error_catalog: ok\n";
}

// ===================================================================
// 12. Versioned envelope forward compatibility
// ===================================================================
void test_versioned_envelope_forward_compat() {
    // Forward compatibility: a serialized envelope from a hypothetical
    // future version can still be deserialized if the wire format is
    // backward-compatible (version field is checked after parsing).
    //
    // The current implementation rejects unknown versions via valid(),
    // so future versions must be negotiated at the protocol level.
    // This test documents that contract.

    // Envelope with version 0 (invalid) is rejected
    auto result = wish::deserialize_envelope(
        "VERSION:0\n"
        "CODE:1001\n"
        "INCIDENT:ABCD\n"
        "MKEY:test\n"
    );
    assert(!result.has_value());

    // Envelope with version > current is rejected
    auto result2 = wish::deserialize_envelope(
        "VERSION:99\n"
        "CODE:1001\n"
        "INCIDENT:ABCD\n"
        "MKEY:test\n"
    );
    assert(!result2.has_value());

    // Missing version is rejected
    auto result3 = wish::deserialize_envelope(
        "CODE:1001\n"
        "INCIDENT:ABCD\n"
        "MKEY:test\n"
    );
    assert(!result3.has_value());

    // Future unknown fields are silently ignored (forward compatibility)
    auto result4 = wish::deserialize_envelope(
        "VERSION:1\n"
        "CODE:2001\n"
        "INCIDENT:CAFE1234\n"
        "MKEY:errors.session.expired\n"
        "NEW_FIELD:introducing_new_capability\n"
        "ANOTHER_NEW:some_value\n"
    );
    assert(result4.has_value());
    assert(result4->error_code == 2001);
    assert(result4->incident_id == "CAFE1234");
    assert(result4->message_key == "errors.session.expired");
    assert(result4->version == 1);

    std::cout << "  versioned_envelope_forward_compat: ok\n";
}

// ===================================================================
// 12. Admin contracts (game-neutral)
// ===================================================================
void test_admin_contracts() {
    // AdminCommand struct compiles and has expected defaults
    wish::admin::AdminCommand cmd {};
    assert(cmd.name.empty());
    assert(cmd.description.empty());

    // HeartbeatService compiles and links
    wish::admin::HeartbeatService service {};
    assert(service.get_servers().empty());
    assert(service.is_alive("nonexistent") == false);

    // Metrics struct compiles
    wish::admin::Metrics metrics {};
    assert(metrics.active_sessions == 0);
    assert(metrics.active_activities == 0);

    // MetricsCollector compiles and links
    wish::admin::MetricsCollector collector {};
    collector.record(metrics);
    const std::string prom = collector.render_prometheus();
    assert(!prom.empty());

    std::cout << "  admin_contracts: ok\n";
}

} // anonymous namespace

int main() {
    std::cout << "Wish SDK out-of-tree consumer test\n";
    std::cout << "==================================\n";

    test_types_standalone();
    test_sequence_tracker();
    test_log();
    test_engine_identity();
    test_session_model();
    test_session_group();
    test_replication_frame();
    test_transport_config();
    test_session_services();
    test_error_envelope();
    test_error_catalog();
    test_versioned_envelope_forward_compat();
    test_admin_contracts();

    std::cout << "==================================\n";
    std::cout << "All Wish SDK consumer tests passed.\n";
    return 0;
}
