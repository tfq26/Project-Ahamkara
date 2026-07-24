// ── Wish independent consumer smoke test ─────────────────────────────────────
//
// This executable proves that Wish builds, links, and runs without any
// dependency on engine/, game/, client/, server/, or samples/flashback/
// source trees.  It links only Wish::wish_engine and uses only Wish
// public headers.
//
// See docs/wish/architecture.md for the boundary contract.
// ─────────────────────────────────────────────────────────────────────────────

#include "wish/core/engine_identity.h"
#include "wish/core/session_services.h"
#include "wish/core/error_codes.h"
#include "wish/core/error_envelope.h"
#include "wish/core/error_catalog.h"
#include "wish/net/transport_config.h"
#include "wish/replication/replication_frame.h"
#include "wish/session/session_model.h"
#include "wish/session/session_runtime.h"
#include "wish/session/session_group.h"
#include "wish/admin/heartbeat_service.h"
#include "wish/admin/admin_command.h"
#include "wish/admin/metrics_collector.h"
#include "wish/types.h"
#include "wish/log.h"

#include <cassert>
#include <iostream>
#include <string_view>

// Verify that no Ahamkara or Flashback headers leak through.
#ifdef AHAMKARA_CORE_H
#error "Ahamkara header leaked into Wish public interface"
#endif
#ifdef AHAMKARA_GAME_NET_TYPES_H
#error "Flashback/game header leaked into Wish public interface"
#endif

namespace {

void test_types_standalone() {
    static_assert(sizeof(wish::u8) == 1, "wish::u8 must be 1 byte");
    static_assert(sizeof(wish::u16) == 2, "wish::u16 must be 2 bytes");
    static_assert(sizeof(wish::u32) == 4, "wish::u32 must be 4 bytes");
    static_assert(sizeof(wish::u64) == 8, "wish::u64 must be 8 bytes");

    wish::NetAddress addr1 {"127.0.0.1", 7777};
    assert(addr1.ip == "127.0.0.1");
    assert(addr1.port == 7777);

    wish::NetAddress addr2 {"127.0.0.1", 7777};
    wish::NetAddress addr3 {"127.0.0.1", 7778};
    assert(addr1 == addr2);
    assert(addr1 != addr3);

    assert(wish::trim("  hello  ") == "hello");
    assert(wish::trim("") == "");

    std::cout << "test_types_standalone: ok\n";
}

void test_log_standalone() {
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

void test_error_codes_and_envelope() {
    // Verify error code format
    char buf[12] {};
    wish::format_wish_code(wish::WishErrorCode::kAuthRejected, buf, sizeof(buf));
    assert(std::string_view(buf) == "WS-AUT-1001");

    wish::format_wish_code(wish::WishErrorCode::kSessionExpired, buf, sizeof(buf));
    assert(std::string_view(buf) == "WS-SES-2001");

    // Verify envelope serialization round-trip
    wish::ErrorEnvelope envelope {};
    envelope.error_code = static_cast<std::uint32_t>(wish::WishErrorCode::kAuthRejected);
    envelope.incident_id = "7F4A19C2";
    envelope.message_key = "errors.auth.rejected";
    envelope.retryable = true;
    envelope.retry_after_seconds = 5;

    assert(envelope.valid());
    assert(envelope.has_error());

    const std::string serialized = wish::serialize_envelope(envelope);
    assert(!serialized.empty());

    const auto deserialized = wish::deserialize_envelope(serialized);
    assert(deserialized.has_value());
    assert(deserialized->error_code == envelope.error_code);
    assert(deserialized->incident_id == envelope.incident_id);
    assert(deserialized->message_key == envelope.message_key);
    assert(deserialized->retryable == envelope.retryable);
    assert(deserialized->retry_after_seconds == envelope.retry_after_seconds);

    // Verify catalog lookup
    const auto& catalog = wish::ErrorCatalog::instance();
    const auto* entry = catalog.find(wish::WishErrorCode::kAuthRejected);
    assert(entry != nullptr);
    assert(std::string_view(entry->domain) == wish::WishDomain::kAuth);

    std::cout << "test_error_codes_and_envelope: ok\n";
}

void test_session_model_is_neutral() {
    wish::session::SessionModel model {};
    assert(model.id.value == 0);
    assert(!model.connected);
    assert(model.server_tick == 0);
    std::cout << "test_session_model_is_neutral: ok\n";
}

void test_session_runtime() {
    wish::session::SessionRuntime runtime(std::chrono::seconds(10));
    const auto now = wish::session::SessionRuntime::clock::time_point {};

    const wish::NetAddress addr {"127.0.0.1", 30001};
    auto& client = runtime.touch_client(addr, now);
    assert(client.identity == "127.0.0.1:30001");
    assert(client.connection_state == wish::session::ClientConnectionState::PendingAdmission);
    assert(runtime.client_count() == 1);

    runtime.prune_timed_out_clients(now + std::chrono::seconds(15));
    assert(runtime.client_count() == 0);

    std::cout << "test_session_runtime: ok\n";
}

void test_session_group() {
    wish::session::SessionGroup group(1, 4);
    const auto now = wish::session::SessionGroup::clock::time_point {};

    const wish::NetAddress addr {"127.0.0.1", 30002};
    auto* client = group.add_client(addr, now);
    assert(client != nullptr);
    assert(group.client_count() == 1);

    group.remove_client(addr);
    assert(group.client_count() == 0);

    std::cout << "test_session_group: ok\n";
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

void test_heartbeat_service() {
    wish::admin::HeartbeatService svc(std::chrono::seconds(30));
    svc.report_heartbeat("server-1", "127.0.0.1", 7777);

    const auto servers = svc.get_servers();
    assert(servers.size() == 1);
    assert(servers[0].alive);

    std::cout << "test_heartbeat_service: ok\n";
}

void test_metrics_collector() {
    wish::admin::MetricsCollector collector;
    wish::admin::Metrics metrics {};
    metrics.active_sessions = 5;
    metrics.active_activities = 2;
    metrics.uptime_seconds = 3600;

    collector.record(metrics);
    const std::string prom = collector.render_prometheus();

    assert(prom.find("wish_active_sessions 5") != std::string_view::npos);
    assert(prom.find("wish_active_activities 2") != std::string_view::npos);
    assert(prom.find("wish_uptime_seconds 3600") != std::string_view::npos);

    std::cout << "test_metrics_collector: ok\n";
}

void test_admin_command() {
    wish::admin::AdminCommand cmd {"shutdown", "Shuts down the server"};
    assert(cmd.name == "shutdown");
    assert(!cmd.description.empty());
    std::cout << "test_admin_command: ok\n";
}

} // namespace

int main() {
    test_types_standalone();
    test_log_standalone();
    test_engine_identity();
    test_error_codes_and_envelope();
    test_session_model_is_neutral();
    test_session_runtime();
    test_session_group();
    test_replication_frame_is_template();
    test_transport_config();
    test_session_services_are_pure_wish();
    test_heartbeat_service();
    test_metrics_collector();
    test_admin_command();

    std::cout << "\nAll Wish consumer tests passed —"
              << " no Ahamkara or Flashback dependencies.\n";
    return 0;
}
