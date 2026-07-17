#include "wish/core/engine_identity.h"
#include "wish/core/session_services.h"
#include "wish/net/transport_config.h"
#include "wish/replication/replication_frame.h"
#include "wish/session/session_model.h"

#include <cassert>
#include <iostream>

namespace {

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
    test_engine_identity();
    test_session_model_is_neutral();
    test_replication_frame_is_template();
    test_transport_config();
    test_session_services_are_pure_wish();
    std::cout << "All wish_consumer tests passed.\n";
    return 0;
}
