// Auth fail-closed tests.
//
// Verifies that authentication fails closed outside explicit development
// mode, and that the dev mode flag is correctly detected.

#include "wish/core/dev_mode.h"
#include "wish/core/session_services.h"
#include "wish/core/error_codes.h"
#include "wish/core/error_envelope.h"
#include "wish/integrations/nakama/mock_session_services.h"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>

static int g_failures = 0;

#define EXPECT_TRUE(cond)                                                                \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " << #cond << "\n"; \
            ++g_failures;                                                                \
        }                                                                                \
    } while (0)

#define EXPECT_FALSE(cond)                                                               \
    do {                                                                                 \
        if ((cond)) {                                                                    \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " << #cond << "\n"; \
            ++g_failures;                                                                \
        }                                                                                \
    } while (0)

#define EXPECT_EQ(a, b)                                                                      \
    do {                                                                                     \
        if ((a) != (b)) {                                                                    \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " << #a << " == " << #b \
                      << " (" << (a) << " != " << (b) << ")\n";                              \
            ++g_failures;                                                                    \
        }                                                                                    \
    } while (0)

namespace {

// ===================================================================
// Dev mode detection
// ===================================================================
void test_dev_mode_default_is_false() {
    // When WISH_DEV_MODE is not set, is_dev_mode() must return false
    // (fail-closed default).
    EXPECT_FALSE(wish::core::is_dev_mode());
}

void test_dev_mode_env_var_1() {
    ::setenv("WISH_DEV_MODE", "1", 1);
    EXPECT_TRUE(wish::core::is_dev_mode());
    ::unsetenv("WISH_DEV_MODE");
}

void test_dev_mode_env_var_true() {
    ::setenv("WISH_DEV_MODE", "true", 1);
    EXPECT_TRUE(wish::core::is_dev_mode());
    ::unsetenv("WISH_DEV_MODE");
}

void test_dev_mode_env_var_yes() {
    ::setenv("WISH_DEV_MODE", "yes", 1);
    EXPECT_TRUE(wish::core::is_dev_mode());
    ::unsetenv("WISH_DEV_MODE");
}

void test_dev_mode_env_var_invalid_is_false() {
    ::setenv("WISH_DEV_MODE", "0", 1);
    EXPECT_FALSE(wish::core::is_dev_mode());
    ::unsetenv("WISH_DEV_MODE");

    ::setenv("WISH_DEV_MODE", "false", 1);
    EXPECT_FALSE(wish::core::is_dev_mode());
    ::unsetenv("WISH_DEV_MODE");

    ::setenv("WISH_DEV_MODE", "any_random_value", 1);
    EXPECT_FALSE(wish::core::is_dev_mode());
    ::unsetenv("WISH_DEV_MODE");
}

// ===================================================================
// DenyAllAuthValidator (fail-closed)
// ===================================================================
void test_deny_all_auth_validator_rejects() {
    wish::integrations::nakama::DenyAllAuthValidator validator {};

    wish::core::AuthRequest req {"some-token", "127.0.0.1:7777"};
    auto result = validator.validate(req);

    EXPECT_FALSE(result.accepted);
    EXPECT_TRUE(result.player_id.empty());
    EXPECT_TRUE(result.session_id.empty());
    EXPECT_FALSE(result.error_message.empty());
    std::cout << "  deny_all_auth_validator: rejected (expected) msg='" << result.error_message << "'\n";
}

void test_deny_all_auth_validator_rejects_empty() {
    wish::integrations::nakama::DenyAllAuthValidator validator {};

    wish::core::AuthRequest req {};
    auto result = validator.validate(req);

    EXPECT_FALSE(result.accepted);
}

// ===================================================================
// NoopAuthValidator (development mode only)
// ===================================================================
void test_noop_auth_validator_accepts() {
    wish::integrations::nakama::NoopAuthValidator validator {};

    wish::core::AuthRequest req {"test-token", "127.0.0.1:7777"};
    auto result = validator.validate(req);

    EXPECT_TRUE(result.accepted);
    EXPECT_FALSE(result.player_id.empty());
    EXPECT_FALSE(result.session_id.empty());
    EXPECT_TRUE(result.error_message.empty());
    std::cout << "  noop_auth_validator: accepted (expected in dev mode)\n";
}

void test_noop_auth_validator_generates_ids() {
    wish::integrations::nakama::NoopAuthValidator validator {};

    wish::core::AuthRequest req {"my-custom-token", "10.0.0.1:9999"};
    auto result = validator.validate(req);

    EXPECT_TRUE(result.accepted);
    EXPECT_EQ(result.player_id, "wish-player@10.0.0.1:9999");
    EXPECT_EQ(result.session_id, "my-custom-token");
}

// ===================================================================
// Error envelope integration with auth failures
// ===================================================================
void test_auth_error_envelope() {
    wish::ErrorEnvelope env;
    env.error_code = static_cast<std::uint32_t>(wish::WishErrorCode::kAuthRejected);
    env.incident_id = "DEADBEEF";
    env.message_key = "errors.auth.rejected";
    env.retryable = true;
    env.retry_after_seconds = 5;

    EXPECT_TRUE(env.valid());
    EXPECT_TRUE(env.has_error());

    // Serialize and deserialize
    auto serialized = wish::serialize_envelope(env);
    EXPECT_FALSE(serialized.empty());

    auto deserialized = wish::deserialize_envelope(serialized);
    EXPECT_TRUE(deserialized.has_value());
    EXPECT_EQ(deserialized->error_code, static_cast<std::uint32_t>(wish::WishErrorCode::kAuthRejected));
    EXPECT_TRUE(deserialized->retryable);
    EXPECT_EQ(deserialized->retry_after_seconds, 5u);

    std::cout << "  auth_error_envelope: ok\n";
}

} // anonymous namespace

int main() {
    std::cout << "auth_tests\n";
    std::cout << "==========\n";

    test_dev_mode_default_is_false();
    test_dev_mode_env_var_1();
    test_dev_mode_env_var_true();
    test_dev_mode_env_var_yes();
    test_dev_mode_env_var_invalid_is_false();
    test_deny_all_auth_validator_rejects();
    test_deny_all_auth_validator_rejects_empty();
    test_noop_auth_validator_accepts();
    test_noop_auth_validator_generates_ids();
    test_auth_error_envelope();

    if (g_failures != 0) {
        std::cerr << "auth_tests failures=" << g_failures << "\n";
        return 1;
    }
    std::cout << "auth_tests: all passed\n";
    return 0;
}
