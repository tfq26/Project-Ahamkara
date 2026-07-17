// Flashback error system tests.
//
// Covers FB-* error codes, AE-*/WS-* external mapping, unknown codes,
// redaction, retry/reconnect/fatal recovery paths, and presentation output.

#include "flashback/flashback_error_codes.h"
#include "flashback/flashback_error_catalog.h"
#include "flashback/flashback_error_presentation.h"

#include <cassert>
#include <cstring>
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

#define EXPECT_EQ(a, b)                                                                      \
    do {                                                                                     \
        if ((a) != (b)) {                                                                    \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " << #a << " == " << #b \
                      << " (" << (a) << " != " << (b) << ")\n";                              \
            ++g_failures;                                                                    \
        }                                                                                    \
    } while (0)

#define EXPECT_NE(a, b)                                                                      \
    do {                                                                                     \
        if ((a) == (b)) {                                                                    \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " << #a << " != " << #b \
                      << " (" << (a) << " == " << (b) << ")\n";                              \
            ++g_failures;                                                                    \
        }                                                                                    \
    } while (0)

#define EXPECT_STREQ(a, b)                                                           \
    do {                                                                             \
        if (std::strcmp((a), (b)) != 0) {                                            \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " strcmp(" << #a  \
                      << ", " << #b << ") == 0  (" << ((a) ? (a) : "null") << " != " \
                      << ((b) ? (b) : "null") << ")\n";                              \
            ++g_failures;                                                            \
        }                                                                            \
    } while (0)

namespace {

// ============================================================================
// FB-* error code tests
// ============================================================================

void test_flashback_error_code_enum_values() {
    EXPECT_EQ(static_cast<int>(flashback::FlashbackErrorCode::kContentIncompatible), 1001);
    EXPECT_EQ(static_cast<int>(flashback::FlashbackErrorCode::kContentRepairRequired), 1002);
    EXPECT_EQ(static_cast<int>(flashback::FlashbackErrorCode::kReconnectRequired), 2001);
    EXPECT_EQ(static_cast<int>(flashback::FlashbackErrorCode::kMutedAudioFallback), 3001);
    EXPECT_EQ(static_cast<int>(flashback::FlashbackErrorCode::kFatalBootFailure), 4001);
}

void test_flashback_code_domain() {
    EXPECT_STREQ(flashback::flashback_code_domain(flashback::FlashbackErrorCode::kContentIncompatible), "GME");
    EXPECT_STREQ(flashback::flashback_code_domain(flashback::FlashbackErrorCode::kContentRepairRequired), "GME");
    EXPECT_STREQ(flashback::flashback_code_domain(flashback::FlashbackErrorCode::kReconnectRequired), "NET");
    EXPECT_STREQ(flashback::flashback_code_domain(flashback::FlashbackErrorCode::kMutedAudioFallback), "AUD");
    EXPECT_STREQ(flashback::flashback_code_domain(flashback::FlashbackErrorCode::kFatalBootFailure), "BOT");
    // Unknown code returns nullptr
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    EXPECT_TRUE(flashback::flashback_code_domain(static_cast<flashback::FlashbackErrorCode>(9999)) == nullptr);
}

void test_format_flashback_code() {
    char buffer[12];

    flashback::format_flashback_code(flashback::FlashbackErrorCode::kContentIncompatible, buffer, sizeof(buffer));
    EXPECT_STREQ(buffer, "FB-GME-1001");

    flashback::format_flashback_code(flashback::FlashbackErrorCode::kReconnectRequired, buffer, sizeof(buffer));
    EXPECT_STREQ(buffer, "FB-NET-2001");

    flashback::format_flashback_code(flashback::FlashbackErrorCode::kMutedAudioFallback, buffer, sizeof(buffer));
    EXPECT_STREQ(buffer, "FB-AUD-3001");

    flashback::format_flashback_code(flashback::FlashbackErrorCode::kFatalBootFailure, buffer, sizeof(buffer));
    EXPECT_STREQ(buffer, "FB-BOT-4001");

    // Unknown code -> empty string
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    flashback::format_flashback_code(static_cast<flashback::FlashbackErrorCode>(9999), buffer, sizeof(buffer));
    EXPECT_EQ(buffer[0], '\0');

    // Buffer too small
    char small[4];
    flashback::format_flashback_code(flashback::FlashbackErrorCode::kContentIncompatible, small, 4);
    // small buffer is left unchanged (function returns early)
}

// ============================================================================
// FB-* error catalog tests
// ============================================================================

void test_catalog_lookup() {
    const auto& catalog = flashback::FlashbackErrorCatalog::instance();

    // All registered codes are found
    EXPECT_TRUE(catalog.find(flashback::FlashbackErrorCode::kContentIncompatible) != nullptr);
    EXPECT_TRUE(catalog.find(flashback::FlashbackErrorCode::kContentRepairRequired) != nullptr);
    EXPECT_TRUE(catalog.find(flashback::FlashbackErrorCode::kReconnectRequired) != nullptr);
    EXPECT_TRUE(catalog.find(flashback::FlashbackErrorCode::kMutedAudioFallback) != nullptr);
    EXPECT_TRUE(catalog.find(flashback::FlashbackErrorCode::kFatalBootFailure) != nullptr);

    // Unknown code returns nullptr
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    EXPECT_TRUE(catalog.find(static_cast<flashback::FlashbackErrorCode>(0)) == nullptr);
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    EXPECT_TRUE(catalog.find(static_cast<flashback::FlashbackErrorCode>(9999)) == nullptr);
}

void test_catalog_entry_fields() {
    const auto& catalog = flashback::FlashbackErrorCatalog::instance();

    const auto* entry = catalog.find(flashback::FlashbackErrorCode::kContentIncompatible);
    EXPECT_TRUE(entry != nullptr);
    EXPECT_EQ(entry->code, flashback::FlashbackErrorCode::kContentIncompatible);
    EXPECT_STREQ(entry->domain, "GME");
    EXPECT_STREQ(entry->message_key, "errors.gameplay.content_incompatible");
    EXPECT_STREQ(entry->title, "Content version incompatible");
    EXPECT_STREQ(entry->owner, "flashback/gameplay");
    EXPECT_EQ(static_cast<int>(entry->recovery), static_cast<int>(flashback::FlashbackRecoveryAction::UpdateRequired));
    EXPECT_TRUE(entry->user_visible);
    EXPECT_STREQ(entry->support_slug, "fb-gme-1001");

    // Reconnect should have Reconnect policy
    const auto* net_entry = catalog.find(flashback::FlashbackErrorCode::kReconnectRequired);
    EXPECT_TRUE(net_entry != nullptr);
    EXPECT_EQ(static_cast<int>(net_entry->recovery), static_cast<int>(flashback::FlashbackRecoveryAction::Reconnect));

    // Fatal boot should have FatalShutdown policy
    const auto* boot_entry = catalog.find(flashback::FlashbackErrorCode::kFatalBootFailure);
    EXPECT_TRUE(boot_entry != nullptr);
    EXPECT_EQ(static_cast<int>(boot_entry->recovery), static_cast<int>(flashback::FlashbackRecoveryAction::FatalShutdown));
}

void test_catalog_lookup_by_numeric_value() {
    const auto& catalog = flashback::FlashbackErrorCatalog::instance();

    const auto* entry = catalog.find(1001u);
    EXPECT_TRUE(entry != nullptr);
    EXPECT_EQ(entry->code, flashback::FlashbackErrorCode::kContentIncompatible);

    // Unknown numeric value returns nullptr
    EXPECT_TRUE(catalog.find(0u) == nullptr);
    EXPECT_TRUE(catalog.find(9999u) == nullptr);
}

void test_catalog_size() {
    const auto& catalog = flashback::FlashbackErrorCatalog::instance();
    EXPECT_TRUE(catalog.size() >= 5);
}

// ============================================================================
// External code mapping tests
// ============================================================================

void test_map_external_ae_cfg() {
    auto code = flashback::map_external_to_flashback_code("AE", "CFG");
    EXPECT_EQ(code, flashback::FlashbackErrorCode::kFatalBootFailure);
}

void test_map_external_ae_ast() {
    auto code = flashback::map_external_to_flashback_code("AE", "AST");
    EXPECT_EQ(code, flashback::FlashbackErrorCode::kContentRepairRequired);
}

void test_map_external_ae_net() {
    auto code = flashback::map_external_to_flashback_code("AE", "NET");
    EXPECT_EQ(code, flashback::FlashbackErrorCode::kReconnectRequired);
}

void test_map_external_ae_rnd() {
    auto code = flashback::map_external_to_flashback_code("AE", "RND");
    EXPECT_EQ(code, flashback::FlashbackErrorCode::kFatalBootFailure);
}

void test_map_external_ae_aud() {
    auto code = flashback::map_external_to_flashback_code("AE", "AUD");
    EXPECT_EQ(code, flashback::FlashbackErrorCode::kMutedAudioFallback);
}

void test_map_external_ae_unknown() {
    auto code = flashback::map_external_to_flashback_code("AE", "UNKNOWN");
    EXPECT_EQ(code, flashback::FlashbackErrorCode::kContentRepairRequired);
}

void test_map_external_ws_aut() {
    auto code = flashback::map_external_to_flashback_code("WS", "AUT");
    EXPECT_EQ(code, flashback::FlashbackErrorCode::kReconnectRequired);
}

void test_map_external_ws_ses() {
    auto code = flashback::map_external_to_flashback_code("WS", "SES");
    EXPECT_EQ(code, flashback::FlashbackErrorCode::kReconnectRequired);
}

void test_map_external_ws_cap() {
    auto code = flashback::map_external_to_flashback_code("WS", "CAP");
    EXPECT_EQ(code, flashback::FlashbackErrorCode::kContentRepairRequired);
}

void test_map_external_ws_pro() {
    auto code = flashback::map_external_to_flashback_code("WS", "PRO");
    EXPECT_EQ(code, flashback::FlashbackErrorCode::kContentRepairRequired);
}

void test_map_external_ws_bak() {
    auto code = flashback::map_external_to_flashback_code("WS", "BAK");
    EXPECT_EQ(code, flashback::FlashbackErrorCode::kContentRepairRequired);
}

void test_map_external_ws_int() {
    auto code = flashback::map_external_to_flashback_code("WS", "INT");
    EXPECT_EQ(code, flashback::FlashbackErrorCode::kContentRepairRequired);
}

void test_map_external_unknown_product() {
    auto code = flashback::map_external_to_flashback_code("UNKNOWN", "XXX");
    EXPECT_EQ(code, flashback::FlashbackErrorCode::kContentRepairRequired);
}

// ============================================================================
// Presentation tests
// ============================================================================

void test_present_flashback_error_has_code_and_incident() {
    auto pres = flashback::present_flashback_error(
        flashback::FlashbackErrorCode::kContentIncompatible);

    EXPECT_TRUE(!pres.code_string.empty());
    EXPECT_TRUE(!pres.incident_id.empty());
    EXPECT_TRUE(!pres.message_key.empty());
    EXPECT_TRUE(!pres.title.empty());
    EXPECT_TRUE(!pres.action_label.empty());
    EXPECT_STREQ(pres.code_string.c_str(), "FB-GME-1001");
}

void test_present_flashback_error_with_incident_id() {
    auto pres = flashback::present_flashback_error(
        flashback::FlashbackErrorCode::kReconnectRequired, "ABCD-1234");

    EXPECT_EQ(pres.incident_id, "ABCD-1234");
    EXPECT_EQ(static_cast<int>(pres.recovery), static_cast<int>(flashback::FlashbackRecoveryAction::Reconnect));
    EXPECT_EQ(pres.action_label, "Reconnect");
}

void test_present_flashback_error_fatal() {
    auto pres = flashback::present_flashback_error(
        flashback::FlashbackErrorCode::kFatalBootFailure);

    EXPECT_EQ(static_cast<int>(pres.recovery), static_cast<int>(flashback::FlashbackRecoveryAction::FatalShutdown));
    EXPECT_EQ(pres.action_label, "Close Flashback");
    EXPECT_STREQ(pres.code_string.c_str(), "FB-BOT-4001");
}

void test_present_external_ae_net() {
    auto pres = flashback::present_external_error("AE", "NET");

    EXPECT_EQ(static_cast<int>(pres.recovery), static_cast<int>(flashback::FlashbackRecoveryAction::Reconnect));
    EXPECT_EQ(pres.action_label, "Reconnect");
    EXPECT_TRUE(!pres.incident_id.empty());
    EXPECT_TRUE(!pres.code_string.empty());
}

void test_present_external_unknown_code() {
    // Unknown external codes degrade to safe generic presentation
    auto pres = flashback::present_external_error("UNKNOWN", "FOO");

    EXPECT_EQ(static_cast<int>(pres.recovery), static_cast<int>(flashback::FlashbackRecoveryAction::RepairContent));
    EXPECT_TRUE(!pres.incident_id.empty());
    EXPECT_TRUE(!pres.code_string.empty());
}

void test_present_external_with_incident_id() {
    auto pres = flashback::present_external_error("WS", "AUT", "DEAD-BEEF");
    EXPECT_EQ(pres.incident_id, "DEAD-BEEF");
    EXPECT_EQ(static_cast<int>(pres.recovery), static_cast<int>(flashback::FlashbackRecoveryAction::Reconnect));
}

void test_present_from_context() {
    flashback::FlashbackErrorContext ctx;
    ctx.external_code = "WS-AUT-1001";
    ctx.incident_id = "CAFE-1234";
    ctx.message_key = "errors.auth.rejected";
    ctx.retryable = true;
    ctx.retry_after_ms = 5000;

    auto pres = flashback::present_from_context(ctx);
    EXPECT_EQ(pres.incident_id, "CAFE-1234");
    // WS-AUT maps to ReconnectRequired -> Reconnect action
    EXPECT_EQ(static_cast<int>(pres.recovery), static_cast<int>(flashback::FlashbackRecoveryAction::Reconnect));
    // Context message key should be used
    EXPECT_EQ(pres.message_key, "errors.auth.rejected");
}

void test_present_from_context_no_code() {
    // Context with no external code -> generic presentation
    flashback::FlashbackErrorContext ctx;
    ctx.incident_id = "0000-0000";

    auto pres = flashback::present_from_context(ctx);
    EXPECT_EQ(pres.incident_id, "0000-0000");
    EXPECT_TRUE(!pres.code_string.empty());
}

// ============================================================================
// Redaction tests
// ============================================================================

void test_redact_path_absolute() {
    auto result = flashback::redact_value("/home/user/sensitive/file.txt");
    EXPECT_EQ(result, "[REDACTED_PATH]");
}

void test_redact_path_relative() {
    auto result = flashback::redact_value("./config/settings.json");
    EXPECT_EQ(result, "[REDACTED_PATH]");
}

void test_redact_path_home() {
    auto result = flashback::redact_value("~/.ssh/id_rsa");
    EXPECT_EQ(result, "[REDACTED_PATH]");
}

void test_redact_url() {
    auto result = flashback::redact_value("https://internal.example.com/token");
    EXPECT_EQ(result, "[REDACTED_PATH]");
}

void test_redact_ip_address() {
    auto result = flashback::redact_value("192.168.1.1");
    EXPECT_EQ(result, "[REDACTED_IP]");
}

void test_redact_stack_trace_pattern() {
    auto result = flashback::redact_value("0x7ffee3b8c000");
    EXPECT_EQ(result, "[REDACTED_TRACE]");
}

void test_redact_token() {
    auto result = flashback::redact_value("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnop");
    EXPECT_EQ(result, "[REDACTED_TOKEN]");
}

void test_redact_safe_string_passes_through() {
    auto result = flashback::redact_value("Hello World");
    EXPECT_EQ(result, "Hello World");
}

void test_redact_empty_string() {
    auto result = flashback::redact_value("");
    EXPECT_EQ(result, "");
}

void test_redact_short_alphanumeric() {
    auto result = flashback::redact_value("abc123");
    EXPECT_EQ(result, "abc123");
}

void test_redact_incident_id_pattern() {
    // "7F4A-19C2" is < 32 chars so passes through
    auto result = flashback::redact_value("7F4A-19C2");
    EXPECT_EQ(result, "7F4A-19C2");
}

// ============================================================================
// Recovery action tests
// ============================================================================

void test_recovery_action_labels() {
    EXPECT_STREQ(flashback::recovery_action_label(flashback::FlashbackRecoveryAction::None).data(), "Close");
    EXPECT_STREQ(flashback::recovery_action_label(flashback::FlashbackRecoveryAction::RetryNow).data(), "Retry");
    EXPECT_STREQ(flashback::recovery_action_label(flashback::FlashbackRecoveryAction::Reconnect).data(), "Reconnect");
    EXPECT_STREQ(flashback::recovery_action_label(flashback::FlashbackRecoveryAction::RepairContent).data(), "Repair Content");
    EXPECT_STREQ(flashback::recovery_action_label(flashback::FlashbackRecoveryAction::UpdateRequired).data(), "Update Required");
    EXPECT_STREQ(flashback::recovery_action_label(flashback::FlashbackRecoveryAction::MutedFallback).data(), "Continue with muted audio");
    EXPECT_STREQ(flashback::recovery_action_label(flashback::FlashbackRecoveryAction::FatalShutdown).data(), "Close Flashback");
}

void test_is_destructive_recovery() {
    EXPECT_TRUE(flashback::is_destructive_recovery(flashback::FlashbackRecoveryAction::RepairContent));
    EXPECT_TRUE(flashback::is_destructive_recovery(flashback::FlashbackRecoveryAction::UpdateRequired));
    EXPECT_TRUE(flashback::is_destructive_recovery(flashback::FlashbackRecoveryAction::FatalShutdown));

    EXPECT_TRUE(!flashback::is_destructive_recovery(flashback::FlashbackRecoveryAction::None));
    EXPECT_TRUE(!flashback::is_destructive_recovery(flashback::FlashbackRecoveryAction::RetryNow));
    EXPECT_TRUE(!flashback::is_destructive_recovery(flashback::FlashbackRecoveryAction::Reconnect));
    EXPECT_TRUE(!flashback::is_destructive_recovery(flashback::FlashbackRecoveryAction::MutedFallback));
}

void test_retry_metadata_in_presentation() {
    // Service-side errors check retry guidance before prescribing destructive changes.
    // WS-BAK maps to ContentRepairRequired which is RepairContent (destructive).
    // But when context says retryable, the presentation layer should respect that.
    flashback::FlashbackErrorContext ctx;
    ctx.external_code = "WS-BAK-5001";
    ctx.incident_id = "RETRY-0001";
    ctx.retryable = true;
    ctx.retry_after_ms = 30000;

    auto pres = flashback::present_from_context(ctx);
    // With retryable=true, recovery should be RetryNow (non-destructive)
    // rather than RepairContent (destructive)
    EXPECT_EQ(static_cast<int>(pres.recovery), static_cast<int>(flashback::FlashbackRecoveryAction::RetryNow));
    EXPECT_EQ(pres.action_label, "Retry");
}

void test_connectivity_recovery_path() {
    // Test the full path: WS-AUT -> ReconnectRequired -> Reconnect action
    auto pres = flashback::present_external_error("WS", "AUT", "CONN-0001");
    EXPECT_EQ(static_cast<int>(pres.recovery), static_cast<int>(flashback::FlashbackRecoveryAction::Reconnect));
    EXPECT_EQ(pres.action_label, "Reconnect");
    EXPECT_TRUE(!flashback::is_destructive_recovery(pres.recovery));
}

void test_fatal_boot_recovery_path() {
    // Test the full path: AE-CFG -> FatalBootFailure -> FatalShutdown
    auto pres = flashback::present_external_error("AE", "CFG", "BOOT-0001");
    EXPECT_EQ(static_cast<int>(pres.recovery), static_cast<int>(flashback::FlashbackRecoveryAction::FatalShutdown));
    EXPECT_EQ(pres.action_label, "Close Flashback");
    EXPECT_TRUE(flashback::is_destructive_recovery(pres.recovery));
}

void test_content_repair_path() {
    // FB-GME-1002 -> RepairContent -> destructive
    auto pres = flashback::present_flashback_error(
        flashback::FlashbackErrorCode::kContentRepairRequired, "REPR-0001");
    EXPECT_EQ(static_cast<int>(pres.recovery), static_cast<int>(flashback::FlashbackRecoveryAction::RepairContent));
    EXPECT_EQ(pres.action_label, "Repair Content");
    EXPECT_TRUE(flashback::is_destructive_recovery(pres.recovery));
}

void test_muted_audio_path() {
    // FB-AUD-3001 -> MutedFallback -> non-destructive
    auto pres = flashback::present_flashback_error(
        flashback::FlashbackErrorCode::kMutedAudioFallback, "AUDI-0001");
    EXPECT_EQ(static_cast<int>(pres.recovery), static_cast<int>(flashback::FlashbackRecoveryAction::MutedFallback));
    EXPECT_EQ(pres.action_label, "Continue with muted audio");
    EXPECT_TRUE(!flashback::is_destructive_recovery(pres.recovery));
}

void test_localization_can_change_without_changing_code() {
    // Code identity is stable; message_key is the localization handle.
    // Changing the catalog entry's message_key doesn't change the code identity.
    const auto& catalog = flashback::FlashbackErrorCatalog::instance();
    const auto* entry = catalog.find(flashback::FlashbackErrorCode::kContentIncompatible);
    EXPECT_TRUE(entry != nullptr);

    // The code string is "FB-GME-1001" regardless of what the title says
    char buf[12];
    flashback::format_flashback_code(entry->code, buf, sizeof(buf));
    EXPECT_STREQ(buf, "FB-GME-1001");

    // message_key may change independently
    EXPECT_STREQ(entry->message_key, "errors.gameplay.content_incompatible");
}

void test_no_internal_paths_in_presentation() {
    // Stack traces, paths, credentials should never appear in presentation
    auto pres = flashback::present_flashback_error(
        flashback::FlashbackErrorCode::kContentRepairRequired);

    // Presentation should not contain common path indicators
    EXPECT_TRUE(pres.code_string.find('/') == std::string::npos);
    EXPECT_TRUE(pres.code_string.find('\\') == std::string::npos);
    EXPECT_TRUE(pres.title.find('/') == std::string::npos);
    EXPECT_TRUE(pres.incident_id.find('/') == std::string::npos);
    EXPECT_TRUE(pres.support_slug.find('/') == std::string::npos);
}

} // anonymous namespace

int main() {
    // FB-* code tests
    test_flashback_error_code_enum_values();
    test_flashback_code_domain();
    test_format_flashback_code();

    // Catalog tests
    test_catalog_lookup();
    test_catalog_entry_fields();
    test_catalog_lookup_by_numeric_value();
    test_catalog_size();

    // External code mapping tests
    test_map_external_ae_cfg();
    test_map_external_ae_ast();
    test_map_external_ae_net();
    test_map_external_ae_rnd();
    test_map_external_ae_aud();
    test_map_external_ae_unknown();
    test_map_external_ws_aut();
    test_map_external_ws_ses();
    test_map_external_ws_cap();
    test_map_external_ws_pro();
    test_map_external_ws_bak();
    test_map_external_ws_int();
    test_map_external_unknown_product();

    // Presentation tests
    test_present_flashback_error_has_code_and_incident();
    test_present_flashback_error_with_incident_id();
    test_present_flashback_error_fatal();
    test_present_external_ae_net();
    test_present_external_unknown_code();
    test_present_external_with_incident_id();
    test_present_from_context();
    test_present_from_context_no_code();

    // Redaction tests
    test_redact_path_absolute();
    test_redact_path_relative();
    test_redact_path_home();
    test_redact_url();
    test_redact_ip_address();
    test_redact_stack_trace_pattern();
    test_redact_token();
    test_redact_safe_string_passes_through();
    test_redact_empty_string();
    test_redact_short_alphanumeric();
    test_redact_incident_id_pattern();

    // Recovery action tests
    test_recovery_action_labels();
    test_is_destructive_recovery();
    test_retry_metadata_in_presentation();
    test_connectivity_recovery_path();
    test_fatal_boot_recovery_path();
    test_content_repair_path();
    test_muted_audio_path();
    test_localization_can_change_without_changing_code();
    test_no_internal_paths_in_presentation();

    if (g_failures != 0) {
        std::cerr << "flashback_error_tests failures=" << g_failures << "\n";
        return 1;
    }
    std::cout << "flashback_error_tests: ok\n";
    return 0;
}
