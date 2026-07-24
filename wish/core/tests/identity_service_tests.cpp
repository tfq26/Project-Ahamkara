#include "wish/core/identity_service.h"
#include "wish/integrations/mock_identity_services.h"

#include <iostream>
#include <string>

namespace {

int fail(const std::string& msg) {
    std::cerr << "identity_service_tests failed: " << msg << '\n';
    return 1;
}

#define EXPECT(cond, msg)     \
    do {                      \
        if (!(cond))          \
            return fail(msg); \
    } while (0)

// ── Tests ──────────────────────────────────────────────────────────────────

int test_resolve_identity() {
    wish::integrations::NoopIdentityService svc;

    wish::core::IdentityRequest req;
    req.token = "test-token-123";
    req.remote_endpoint = "127.0.0.1:30001";

    auto result = svc.resolve(req);
    EXPECT(result.found, "identity should be found");
    EXPECT(!result.identity.player_id.empty(), "player_id should not be empty");
    EXPECT(result.identity.display_name == "Player", "display_name should be 'Player'");
    EXPECT(result.identity.realm == "local", "realm should be 'local'");

    std::cout << "test_resolve_identity: ok\n";
    return 0;
}

int test_resolve_with_empty_endpoint() {
    wish::integrations::NoopIdentityService svc;

    wish::core::IdentityRequest req;
    req.token = "test-token-456";

    auto result = svc.resolve(req);
    EXPECT(result.found, "identity should be found even with empty endpoint");
    EXPECT(result.identity.player_id == "player-unknown", "should fallback to 'player-unknown'");

    std::cout << "test_resolve_with_empty_endpoint: ok\n";
    return 0;
}

int test_lookup_by_id() {
    wish::integrations::NoopIdentityService svc;

    auto result = svc.lookup_by_id("player-abc123");
    EXPECT(result.found, "lookup should succeed");
    EXPECT(result.identity.player_id == "player-abc123", "player_id should match");
    EXPECT(!result.identity.display_name.empty(), "display_name should not be empty");
    EXPECT(result.identity.realm == "local", "realm should be 'local'");

    std::cout << "test_lookup_by_id: ok\n";
    return 0;
}

int test_lookup_batch() {
    wish::integrations::NoopIdentityService svc;

    std::vector<std::string> ids = {"one", "two", "three"};
    auto results = svc.lookup_batch(ids);
    EXPECT(results.size() == 3, "should return 3 results");

    for (std::size_t i = 0; i < results.size(); ++i) {
        EXPECT(results[i].found, "result " + std::to_string(i) + " should be found");
        EXPECT(results[i].identity.player_id == ids[i], "player_id should match at index " + std::to_string(i));
    }

    std::cout << "test_lookup_batch: ok\n";
    return 0;
}

int test_identity_interface_contract() {
    // Verify the interface contract compiles and is usable
    wish::core::IdentityRequest req;
    req.token = "token";
    req.remote_endpoint = "1.2.3.4:5678";

    wish::core::IdentityResult res;
    res.found = true;
    res.identity.player_id = "pid";
    res.identity.display_name = "name";
    res.identity.realm = "realm";
    res.identity.avatar_url = "https://example.com/avatar.png";

    // Verify result with found=false has empty identity
    wish::core::IdentityResult not_found;
    EXPECT(!not_found.found, "default result should have found=false");
    EXPECT(not_found.identity.player_id.empty(), "default player_id should be empty");

    std::cout << "test_identity_interface_contract: ok\n";
    return 0;
}

} // namespace

int main() {
    int failures = 0;

    failures += test_resolve_identity();
    failures += test_resolve_with_empty_endpoint();
    failures += test_lookup_by_id();
    failures += test_lookup_batch();
    failures += test_identity_interface_contract();

    if (failures > 0) {
        std::cerr << failures << " identity service test(s) FAILED.\n";
        return 1;
    }

    std::cout << "All identity service tests passed.\n";
    return 0;
}
