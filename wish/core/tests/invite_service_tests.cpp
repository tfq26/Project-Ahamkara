#include "wish/core/invite_service.h"
#include "wish/integrations/mock_identity_services.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace {

int fail(const std::string& msg) {
    std::cerr << "invite_service_tests failed: " << msg << '\n';
    return 1;
}

#define EXPECT(cond, msg)     \
    do {                      \
        if (!(cond))          \
            return fail(msg); \
    } while (0)

// ── Tests ──────────────────────────────────────────────────────────────────

int test_send_invite() {
    wish::integrations::NoopInviteService svc;

    wish::core::SendInviteRequest req;
    req.from_player_id = "player-1";
    req.to_player_id = "player-2";
    req.activity_type = "deathmatch";
    req.ttl = std::chrono::seconds(30);

    auto result = svc.send_invite(req);
    EXPECT(result.ok, "send_invite should succeed");
    EXPECT(!result.invite.invite_id.empty(), "invite_id should not be empty");
    EXPECT(result.invite.from_player_id == "player-1", "from should be player-1");
    EXPECT(result.invite.to_player_id == "player-2", "to should be player-2");
    EXPECT(result.invite.status == wish::core::InviteStatus::Pending, "status should be Pending");

    std::cout << "test_send_invite: ok\n";
    return 0;
}

int test_duplicate_invite_rejected() {
    wish::integrations::NoopInviteService svc;

    wish::core::SendInviteRequest req;
    req.from_player_id = "player-1";
    req.to_player_id = "player-2";
    req.activity_type = "deathmatch";

    auto first = svc.send_invite(req);
    EXPECT(first.ok, "first invite should succeed");

    auto second = svc.send_invite(req);
    EXPECT(!second.ok, "duplicate invite should be rejected");

    std::cout << "test_duplicate_invite_rejected: ok\n";
    return 0;
}

int test_accept_invite() {
    wish::integrations::NoopInviteService svc;

    wish::core::SendInviteRequest req;
    req.from_player_id = "player-1";
    req.to_player_id = "player-2";
    req.activity_type = "social_hub";

    auto send_result = svc.send_invite(req);
    EXPECT(send_result.ok, "send should succeed");

    wish::core::RespondInviteRequest respond;
    respond.invite_id = send_result.invite.invite_id;
    respond.player_id = "player-2";
    respond.accept = true;

    auto accept_result = svc.accept_invite(respond);
    EXPECT(accept_result.ok, "accept should succeed");
    EXPECT(accept_result.invite.status == wish::core::InviteStatus::Accepted, "status should be Accepted");

    std::cout << "test_accept_invite: ok\n";
    return 0;
}

int test_reject_invite() {
    wish::integrations::NoopInviteService svc;

    wish::core::SendInviteRequest req;
    req.from_player_id = "player-1";
    req.to_player_id = "player-2";
    req.activity_type = "deathmatch";

    auto send_result = svc.send_invite(req);

    wish::core::RespondInviteRequest respond;
    respond.invite_id = send_result.invite.invite_id;
    respond.player_id = "player-2";
    respond.accept = false;

    auto reject_result = svc.reject_invite(respond);
    EXPECT(reject_result.ok, "reject should succeed");
    EXPECT(reject_result.invite.status == wish::core::InviteStatus::Rejected, "status should be Rejected");

    std::cout << "test_reject_invite: ok\n";
    return 0;
}

int test_cancel_invite() {
    wish::integrations::NoopInviteService svc;

    wish::core::SendInviteRequest req;
    req.from_player_id = "player-1";
    req.to_player_id = "player-2";
    req.activity_type = "deathmatch";

    auto send_result = svc.send_invite(req);

    auto cancel_result = svc.cancel_invite(send_result.invite.invite_id, "player-1");
    EXPECT(cancel_result.ok, "cancel should succeed");
    EXPECT(cancel_result.invite.status == wish::core::InviteStatus::Cancelled, "status should be Cancelled");

    std::cout << "test_cancel_invite: ok\n";
    return 0;
}

int test_cancel_invite_wrong_sender() {
    wish::integrations::NoopInviteService svc;

    wish::core::SendInviteRequest req;
    req.from_player_id = "player-1";
    req.to_player_id = "player-2";
    req.activity_type = "deathmatch";

    auto send_result = svc.send_invite(req);

    // player-2 (recipient) tries to cancel
    auto cancel_result = svc.cancel_invite(send_result.invite.invite_id, "player-2");
    EXPECT(!cancel_result.ok, "non-sender should not be able to cancel");

    std::cout << "test_cancel_invite_wrong_sender: ok\n";
    return 0;
}

int test_get_pending_invites() {
    wish::integrations::NoopInviteService svc;

    wish::core::SendInviteRequest req;
    req.from_player_id = "player-1";
    req.to_player_id = "player-2";
    req.activity_type = "deathmatch";
    (void)svc.send_invite(req);

    // Player 1 should see 1 pending (as sender)
    auto p1_pending = svc.get_pending_invites("player-1");
    EXPECT(p1_pending.size() == 1, "player-1 should have 1 pending invite");

    // Player 2 should see 1 pending (as recipient)
    auto p2_pending = svc.get_pending_invites("player-2");
    EXPECT(p2_pending.size() == 1, "player-2 should see 1 pending invite");

    // Player 3 should see 0
    auto p3_pending = svc.get_pending_invites("player-3");
    EXPECT(p3_pending.empty(), "uninvolved player should have no pending invites");

    std::cout << "test_get_pending_invites: ok\n";
    return 0;
}

int test_outgoing_incoming_invites() {
    wish::integrations::NoopInviteService svc;

    wish::core::SendInviteRequest req;
    req.from_player_id = "player-1";
    req.to_player_id = "player-2";
    req.activity_type = "deathmatch";
    (void)svc.send_invite(req);

    auto outgoing = svc.get_outgoing_invites("player-1");
    EXPECT(outgoing.size() == 1, "player-1 should have 1 outgoing");

    auto incoming = svc.get_incoming_invites("player-2");
    EXPECT(incoming.size() == 1, "player-2 should have 1 incoming");

    std::cout << "test_outgoing_incoming_invites: ok\n";
    return 0;
}

int test_accept_nonexistent_invite() {
    wish::integrations::NoopInviteService svc;

    wish::core::RespondInviteRequest respond;
    respond.invite_id = "nonexistent-id";
    respond.player_id = "player-2";
    respond.accept = true;

    auto result = svc.accept_invite(respond);
    EXPECT(!result.ok, "accepting nonexistent invite should fail");

    std::cout << "test_accept_nonexistent_invite: ok\n";
    return 0;
}

int test_has_pending_invite() {
    wish::integrations::NoopInviteService svc;

    wish::core::SendInviteRequest req;
    req.from_player_id = "player-1";
    req.to_player_id = "player-2";
    req.activity_type = "deathmatch";
    (void)svc.send_invite(req);

    EXPECT(svc.has_pending_invite("player-1", "player-2", "deathmatch"),
           "should detect pending invite");
    EXPECT(!svc.has_pending_invite("player-1", "player-3", "deathmatch"),
           "should not detect invite to different player");
    EXPECT(!svc.has_pending_invite("player-1", "player-2", "social_hub"),
           "should not detect invite for different activity");

    std::cout << "test_has_pending_invite: ok\n";
    return 0;
}

} // namespace

int main() {
    int failures = 0;

    failures += test_send_invite();
    failures += test_duplicate_invite_rejected();
    failures += test_accept_invite();
    failures += test_reject_invite();
    failures += test_cancel_invite();
    failures += test_cancel_invite_wrong_sender();
    failures += test_get_pending_invites();
    failures += test_outgoing_incoming_invites();
    failures += test_accept_nonexistent_invite();
    failures += test_has_pending_invite();

    if (failures > 0) {
        std::cerr << failures << " invite service test(s) FAILED.\n";
        return 1;
    }

    std::cout << "All invite service tests passed.\n";
    return 0;
}
