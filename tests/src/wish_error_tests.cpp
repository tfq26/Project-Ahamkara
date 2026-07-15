#include "wish/core/error_codes.h"
#include "wish/core/error_catalog.h"
#include "wish/core/error_envelope.h"

#include <cassert>
#include <cstring>
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

#define EXPECT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " << #a << " == " << #b \
                      << " (" << (a) << " != " << (b) << ")\n"; \
            ++g_failures; \
        } \
    } while (0)

namespace {

void test_wish_error_code_enum_values() {
    // Verify enum values match expected numeric constants.
    EXPECT_EQ(static_cast<int>(wish::WishErrorCode::kAuthRejected), 1001);
    EXPECT_EQ(static_cast<int>(wish::WishErrorCode::kSessionAdmissionRejected), 1002);
    EXPECT_EQ(static_cast<int>(wish::WishErrorCode::kSessionExpired), 2001);
    EXPECT_EQ(static_cast<int>(wish::WishErrorCode::kActivityUnavailable), 2002);
    EXPECT_EQ(static_cast<int>(wish::WishErrorCode::kCapacityExceeded), 3001);
    EXPECT_EQ(static_cast<int>(wish::WishErrorCode::kProtocolError), 4001);
    EXPECT_EQ(static_cast<int>(wish::WishErrorCode::kProtocolVersionMismatch), 4002);
    EXPECT_EQ(static_cast<int>(wish::WishErrorCode::kBackendUnavailable), 5001);
    EXPECT_EQ(static_cast<int>(wish::WishErrorCode::kBackendTimeout), 5002);
    EXPECT_EQ(static_cast<int>(wish::WishErrorCode::kInternalError), 9001);
}

void test_wish_code_domain() {
    EXPECT_TRUE(std::strcmp(wish::wish_code_domain(wish::WishErrorCode::kAuthRejected), "AUT") == 0);
    EXPECT_TRUE(std::strcmp(wish::wish_code_domain(wish::WishErrorCode::kSessionExpired), "SES") == 0);
    EXPECT_TRUE(std::strcmp(wish::wish_code_domain(wish::WishErrorCode::kCapacityExceeded), "CAP") == 0);
    EXPECT_TRUE(std::strcmp(wish::wish_code_domain(wish::WishErrorCode::kProtocolError), "PRO") == 0);
    EXPECT_TRUE(std::strcmp(wish::wish_code_domain(wish::WishErrorCode::kBackendUnavailable), "BAK") == 0);
    EXPECT_TRUE(std::strcmp(wish::wish_code_domain(wish::WishErrorCode::kInternalError), "INT") == 0);
    // Unknown code returns nullptr
    EXPECT_TRUE(wish::wish_code_domain(static_cast<wish::WishErrorCode>(9999)) == nullptr);
}

void test_format_wish_code() {
    char buffer[12];
    wish::format_wish_code(wish::WishErrorCode::kAuthRejected, buffer, sizeof(buffer));
    EXPECT_TRUE(std::strcmp(buffer, "WS-AUT-1001") == 0);

    wish::format_wish_code(wish::WishErrorCode::kCapacityExceeded, buffer, sizeof(buffer));
    EXPECT_TRUE(std::strcmp(buffer, "WS-CAP-3001") == 0);

    wish::format_wish_code(wish::WishErrorCode::kProtocolVersionMismatch, buffer, sizeof(buffer));
    EXPECT_TRUE(std::strcmp(buffer, "WS-PRO-4002") == 0);

    wish::format_wish_code(wish::WishErrorCode::kBackendTimeout, buffer, sizeof(buffer));
    EXPECT_TRUE(std::strcmp(buffer, "WS-BAK-5002") == 0);

    // Unknown code -> empty string
    wish::format_wish_code(static_cast<wish::WishErrorCode>(9999), buffer, sizeof(buffer));
    EXPECT_EQ(buffer[0], '\0');

    // Buffer too small
    char small[4];
    wish::format_wish_code(wish::WishErrorCode::kAuthRejected, small, 4);
    // small buffer is left unchanged (function returns early)
}

void test_error_envelope_validity() {
    // Valid envelope
    wish::ErrorEnvelope env;
    env.error_code = static_cast<std::uint32_t>(wish::WishErrorCode::kAuthRejected);
    env.incident_id = "7F4A19C2";
    env.message_key = "errors.auth.rejected";
    EXPECT_TRUE(env.valid());

    // Invalid: zero error code
    wish::ErrorEnvelope empty;
    EXPECT_TRUE(!empty.valid());

    // Invalid: wrong version
    wish::ErrorEnvelope bad_ver;
    bad_ver.version = 999;
    bad_ver.error_code = static_cast<std::uint32_t>(wish::WishErrorCode::kAuthRejected);
    EXPECT_TRUE(!bad_ver.valid());

    // Invalid: incident ID too long
    wish::ErrorEnvelope long_incident;
    long_incident.error_code = static_cast<std::uint32_t>(wish::WishErrorCode::kAuthRejected);
    long_incident.incident_id = std::string(wish::kMaxIncidentId + 1, 'A');
    EXPECT_TRUE(!long_incident.valid());

    // Invalid: message key too long
    wish::ErrorEnvelope long_mkey;
    long_mkey.error_code = static_cast<std::uint32_t>(wish::WishErrorCode::kAuthRejected);
    long_mkey.message_key = std::string(wish::kMaxMessageKey + 1, 'x');
    EXPECT_TRUE(!long_mkey.valid());
}

void test_envelope_serialization_round_trip() {
    wish::ErrorEnvelope original;
    original.error_code = static_cast<std::uint32_t>(wish::WishErrorCode::kCapacityExceeded);
    original.incident_id = "A1B2C3D4";
    original.message_key = "errors.capacity.exceeded";
    original.retryable = true;
    original.retry_after_seconds = 30;

    auto serialized = wish::serialize_envelope(original);
    EXPECT_TRUE(!serialized.empty());

    auto deserialized = wish::deserialize_envelope(serialized);
    EXPECT_TRUE(deserialized.has_value());

    EXPECT_EQ(deserialized->version, wish::kErrorEnvelopeVersion);
    EXPECT_EQ(deserialized->error_code, static_cast<std::uint32_t>(wish::WishErrorCode::kCapacityExceeded));
    EXPECT_EQ(deserialized->incident_id, "A1B2C3D4");
    EXPECT_EQ(deserialized->message_key, "errors.capacity.exceeded");
    EXPECT_TRUE(deserialized->retryable);
    EXPECT_EQ(deserialized->retry_after_seconds, 30u);
}

void test_envelope_serialization_round_trip_non_retryable() {
    wish::ErrorEnvelope original;
    original.error_code = static_cast<std::uint32_t>(wish::WishErrorCode::kProtocolVersionMismatch);
    original.incident_id = "00000000";
    original.message_key = "errors.protocol.version_mismatch";
    original.retryable = false;
    original.retry_after_seconds = 0;

    auto serialized = wish::serialize_envelope(original);
    EXPECT_TRUE(!serialized.empty());

    auto deserialized = wish::deserialize_envelope(serialized);
    EXPECT_TRUE(deserialized.has_value());

    EXPECT_EQ(deserialized->error_code, static_cast<std::uint32_t>(wish::WishErrorCode::kProtocolVersionMismatch));
    EXPECT_TRUE(!deserialized->retryable);
    EXPECT_EQ(deserialized->retry_after_seconds, 0u);
}

void test_deserialize_malformed_data() {
    // Empty data
    auto result = wish::deserialize_envelope("");
    EXPECT_TRUE(!result.has_value());

    // Missing version
    auto result2 = wish::deserialize_envelope("CODE:1001\n");
    EXPECT_TRUE(!result2.has_value());

    // Missing code
    auto result3 = wish::deserialize_envelope("VERSION:1\nMKEY:test\n");
    EXPECT_TRUE(!result3.has_value());

    // Gibberish
    auto result4 = wish::deserialize_envelope("this is not valid envelope data\n");
    EXPECT_TRUE(!result4.has_value());
}

void test_deserialize_unknown_field_is_ignored() {
    // Unknown fields should be silently ignored (forward compatibility)
    const std::string data =
        "VERSION:1\n"
        "CODE:1001\n"
        "INCIDENT:ABCD1234\n"
        "MKEY:errors.auth.rejected\n"
        "UNKNOWN_FIELD:some_value\n"
        "RETRY:1\n"
        "RETRY_AFTER:10\n";

    auto result = wish::deserialize_envelope(data);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->error_code, 1001u);
    EXPECT_EQ(result->incident_id, "ABCD1234");
    EXPECT_TRUE(result->retryable);
}

void test_catalog_lookup() {
    const auto& catalog = wish::ErrorCatalog::instance();

    // All registered codes are found
    EXPECT_TRUE(catalog.find(wish::WishErrorCode::kAuthRejected) != nullptr);
    EXPECT_TRUE(catalog.find(wish::WishErrorCode::kSessionAdmissionRejected) != nullptr);
    EXPECT_TRUE(catalog.find(wish::WishErrorCode::kSessionExpired) != nullptr);
    EXPECT_TRUE(catalog.find(wish::WishErrorCode::kActivityUnavailable) != nullptr);
    EXPECT_TRUE(catalog.find(wish::WishErrorCode::kCapacityExceeded) != nullptr);
    EXPECT_TRUE(catalog.find(wish::WishErrorCode::kProtocolError) != nullptr);
    EXPECT_TRUE(catalog.find(wish::WishErrorCode::kProtocolVersionMismatch) != nullptr);
    EXPECT_TRUE(catalog.find(wish::WishErrorCode::kBackendUnavailable) != nullptr);
    EXPECT_TRUE(catalog.find(wish::WishErrorCode::kBackendTimeout) != nullptr);
    EXPECT_TRUE(catalog.find(wish::WishErrorCode::kInternalError) != nullptr);

    // Unknown code returns nullptr
    EXPECT_TRUE(catalog.find(static_cast<wish::WishErrorCode>(0)) == nullptr);
    EXPECT_TRUE(catalog.find(static_cast<wish::WishErrorCode>(9999)) == nullptr);
}

void test_catalog_entry_fields() {
    const auto& catalog = wish::ErrorCatalog::instance();

    const auto* entry = catalog.find(wish::WishErrorCode::kAuthRejected);
    EXPECT_TRUE(entry != nullptr);
    EXPECT_EQ(entry->code, wish::WishErrorCode::kAuthRejected);
    EXPECT_TRUE(std::strcmp(entry->domain, "AUT") == 0);
    EXPECT_TRUE(std::strcmp(entry->message_key, "errors.auth.rejected") == 0);
    EXPECT_TRUE(std::strcmp(entry->title, "Authentication rejected") == 0);
    EXPECT_TRUE(std::strcmp(entry->owner, "wish/auth") == 0);
    EXPECT_EQ(static_cast<int>(entry->recovery), static_cast<int>(wish::RecoveryPolicy::RetryNow));
    EXPECT_TRUE(entry->user_visible);

    // Capacity exceeded should have RetryBackoff policy
    const auto* cap_entry = catalog.find(wish::WishErrorCode::kCapacityExceeded);
    EXPECT_TRUE(cap_entry != nullptr);
    EXPECT_EQ(static_cast<int>(cap_entry->recovery), static_cast<int>(wish::RecoveryPolicy::RetryBackoff));

    // Internal error should have ContactSupport
    const auto* int_entry = catalog.find(wish::WishErrorCode::kInternalError);
    EXPECT_TRUE(int_entry != nullptr);
    EXPECT_EQ(static_cast<int>(int_entry->recovery), static_cast<int>(wish::RecoveryPolicy::ContactSupport));
}

void test_catalog_lookup_by_numeric_value() {
    const auto& catalog = wish::ErrorCatalog::instance();

    const auto* entry = catalog.find(1001u);
    EXPECT_TRUE(entry != nullptr);
    EXPECT_EQ(entry->code, wish::WishErrorCode::kAuthRejected);

    // Unknown numeric value returns nullptr
    EXPECT_TRUE(catalog.find(0u) == nullptr);
    EXPECT_TRUE(catalog.find(9999u) == nullptr);
}

void test_catalog_size() {
    const auto& catalog = wish::ErrorCatalog::instance();
    // kCount-1 active codes (kCount is the sentinel)
    EXPECT_TRUE(catalog.size() >= 10);
}

void test_map_native_nakama_auth() {
    auto code = wish::map_native_to_wish_code("nakama", 3);
    EXPECT_EQ(code, wish::WishErrorCode::kAuthRejected);
}

void test_map_native_nakama_session() {
    auto code = wish::map_native_to_wish_code("nakama", 4);
    EXPECT_EQ(code, wish::WishErrorCode::kSessionExpired);
}

void test_map_native_nakama_capacity() {
    auto code = wish::map_native_to_wish_code("nakama", 5);
    EXPECT_EQ(code, wish::WishErrorCode::kCapacityExceeded);
}

void test_map_native_nakama_unavailable() {
    auto code = wish::map_native_to_wish_code("nakama", 1);
    EXPECT_EQ(code, wish::WishErrorCode::kBackendUnavailable);
}

void test_map_native_nakama_grpc_unavailable() {
    auto code = wish::map_native_to_wish_code("nakama_grpc", 14);
    EXPECT_EQ(code, wish::WishErrorCode::kBackendUnavailable);
}

void test_map_native_nakama_grpc_deadline() {
    auto code = wish::map_native_to_wish_code("nakama_grpc", 4);
    EXPECT_EQ(code, wish::WishErrorCode::kBackendTimeout);
}

void test_map_native_nakama_grpc_unauthenticated() {
    auto code = wish::map_native_to_wish_code("nakama_grpc", 16);
    EXPECT_EQ(code, wish::WishErrorCode::kAuthRejected);
}

void test_map_native_system_errors() {
    EXPECT_EQ(wish::map_native_to_wish_code("system", 60), wish::WishErrorCode::kBackendUnavailable);
    EXPECT_EQ(wish::map_native_to_wish_code("system", 61), wish::WishErrorCode::kBackendUnavailable);
    EXPECT_EQ(wish::map_native_to_wish_code("system", 54), wish::WishErrorCode::kBackendUnavailable);
}

void test_map_native_unknown_domain() {
    auto code = wish::map_native_to_wish_code("unknown_domain", 0);
    EXPECT_EQ(code, wish::WishErrorCode::kInternalError);
}

void test_retry_metadata_round_trip() {
    wish::ErrorEnvelope env;
    env.error_code = static_cast<std::uint32_t>(wish::WishErrorCode::kBackendUnavailable);
    env.incident_id = "DEADBEEF";
    env.message_key = "errors.backend.unavailable";
    env.retryable = true;
    env.retry_after_seconds = 60;

    auto serialized = wish::serialize_envelope(env);
    auto deserialized = wish::deserialize_envelope(serialized);

    EXPECT_TRUE(deserialized.has_value());
    EXPECT_TRUE(deserialized->retryable);
    EXPECT_EQ(deserialized->retry_after_seconds, 60u);
}

void test_retry_metadata_not_retryable() {
    wish::ErrorEnvelope env;
    env.error_code = static_cast<std::uint32_t>(wish::WishErrorCode::kInternalError);
    env.incident_id = "CAFE1234";
    env.message_key = "errors.internal.error";
    env.retryable = false;
    env.retry_after_seconds = 0;

    auto serialized = wish::serialize_envelope(env);
    auto deserialized = wish::deserialize_envelope(serialized);

    EXPECT_TRUE(deserialized.has_value());
    EXPECT_TRUE(!deserialized->retryable);
    EXPECT_EQ(deserialized->retry_after_seconds, 0u);
}

void test_invalid_envelope_rejected_by_serialize() {
    // Empty envelope (no error code) should fail serialization
    wish::ErrorEnvelope invalid;
    auto serialized = wish::serialize_envelope(invalid);
    EXPECT_TRUE(serialized.empty());
}

} // anonymous namespace

int main() {
    test_wish_error_code_enum_values();
    test_wish_code_domain();
    test_format_wish_code();
    test_error_envelope_validity();
    test_envelope_serialization_round_trip();
    test_envelope_serialization_round_trip_non_retryable();
    test_deserialize_malformed_data();
    test_deserialize_unknown_field_is_ignored();
    test_catalog_lookup();
    test_catalog_entry_fields();
    test_catalog_lookup_by_numeric_value();
    test_catalog_size();
    test_map_native_nakama_auth();
    test_map_native_nakama_session();
    test_map_native_nakama_capacity();
    test_map_native_nakama_unavailable();
    test_map_native_nakama_grpc_unavailable();
    test_map_native_nakama_grpc_deadline();
    test_map_native_nakama_grpc_unauthenticated();
    test_map_native_system_errors();
    test_map_native_unknown_domain();
    test_retry_metadata_round_trip();
    test_retry_metadata_not_retryable();
    test_invalid_envelope_rejected_by_serialize();

    if (g_failures != 0) {
        std::cerr << "wish_error_tests failures=" << g_failures << "\n";
        return 1;
    }
    std::cout << "wish_error_tests: ok\n";
    return 0;
}
