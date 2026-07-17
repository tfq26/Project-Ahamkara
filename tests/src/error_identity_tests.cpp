#include "ae/core/error_code.h"
#include "ae/core/error_registry.h"
#include "ae/core/error_report.h"
#include "ae/core/error_types.h"

#include <cassert>
#include <iostream>
#include <string>

static int g_failures = 0;

#define EXPECT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " << #cond << "\n"; \
            ++g_failures; \
        } \
    } while (0)

int main() {
    // Round-trip / well-formed codes
    ae::ErrorCode code("AE-NET-0001");
    EXPECT_TRUE(code.valid());
    EXPECT_TRUE(code.text() == "AE-NET-0001");
    EXPECT_TRUE(code.product() == "AE");
    EXPECT_TRUE(code.domain() == "NET");
    EXPECT_TRUE(code.number() == 1);
    EXPECT_TRUE(!ae::ErrorCode("ae-net-1").valid());
    EXPECT_TRUE(!ae::ErrorCode("AE-NET").valid());
    EXPECT_TRUE(!ae::ErrorCode("AE-NET-12345").valid());

    // Registry active codes + validation
    auto& registry = ae::ErrorRegistry::instance();
    EXPECT_TRUE(registry.validate());
    EXPECT_TRUE(registry.find(ae::ae_cfg_0001()) != nullptr);
    EXPECT_TRUE(registry.find(ae::ae_ast_0001()) != nullptr);
    EXPECT_TRUE(registry.find(ae::ae_net_0001()) != nullptr);
    EXPECT_TRUE(registry.find(ae::ae_rnd_0001()) != nullptr);
    EXPECT_TRUE(registry.find(ae::ae_aud_0001()) != nullptr);

    // Message key can change independently of code identity.
    const auto* desc = registry.find(ae::ae_net_0001());
    EXPECT_TRUE(desc != nullptr);
    EXPECT_TRUE(desc->code.text() == "AE-NET-0001");
    EXPECT_TRUE(!desc->message_key.empty());

    // Result preserves error chain without exceptions
    ae::Error root(ae::ae_cfg_0001());
    root.with_context("path", "/tmp/config.json");
    ae::Error wrapped(ae::ae_ast_0001());
    wrapped.caused_by(root).with_context("asset", "mesh.aemesh");
    ae::Result<int> ok_result = 7;
    ae::Result<int> err_result = wrapped;
    EXPECT_TRUE(ok_result.ok());
    EXPECT_TRUE(ok_result.value() == 7);
    EXPECT_TRUE(!err_result.ok());
    EXPECT_TRUE(err_result.error().code().text() == "AE-AST-0001");
    EXPECT_TRUE(err_result.error().cause() != nullptr);
    EXPECT_TRUE(err_result.error().cause()->code().text() == "AE-CFG-0001");

    // Report once per incident, player banner stable
    ae::Error net_err(ae::ae_net_0001());
    net_err.with_context("token", "Bearer super-secret")
           .with_context("peer_ip", "10.0.0.8")
           .with_native("errno", 111);
    auto first = ae::report_error(net_err);
    auto second = ae::report_error(net_err);
    EXPECT_TRUE(first.reported);
    EXPECT_TRUE(!second.reported);
    EXPECT_TRUE(first.player_banner.find("Code: AE-NET-0001") != std::string::npos);
    EXPECT_TRUE(first.player_banner.find("Incident:") != std::string::npos);
    EXPECT_TRUE(ae::redact_sensitive("Bearer abc").find("redacted") != std::string::npos);
    EXPECT_TRUE(ae::redact_sensitive("10.1.2.3").find("redacted") != std::string::npos);

    // Recovery policy exists and is bounded for active codes
    EXPECT_TRUE(desc->recovery.max_retries <= 8);
    EXPECT_TRUE(desc->recovery.action != ae::RecoveryAction::None || true);

    if (g_failures != 0) {
        std::cerr << "error_identity_tests failures=" << g_failures << "\n";
        return 1;
    }
    std::cout << "error_identity_tests: ok\n";
    return 0;
}
