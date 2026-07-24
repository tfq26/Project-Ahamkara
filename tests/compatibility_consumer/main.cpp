// ==========================================================================
// Compatibility Consumer Smoke
// ==========================================================================
//
// This binary is built in a clean-room environment that has:
//   - NO Ahamkara source checkout
//   - NO Wish source checkout
//   - ONLY Flashback source checkout
//
// It resolves Ahamkara and Wish via find_package(CONFIG) using pre-built
// install artifacts and exercises a bounded subset of the API to prove
// cross-product compatibility.
//
// Compatible builds:  all smoke checks pass, exit 0.
// Incompatible builds: static_assert or runtime abort with a clear message.

#include "ae/core/log.h"
#include "ae/core/time.h"
#include "ae/core/version.h"
#include "ae/network/network_clock.h"
#include "ae/runtime/application.h"
#include "ae/runtime/game_module.h"

#include "wish/core/engine_identity.h"
#include "wish/core/version.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// ==========================================================================
// I. Compile-time version compatibility checks
// ==========================================================================

// Helper for stringification (defined before use in static_assert below)
#define AE_STRINGIFY_IMPL(x) #x
#define AE_STRINGIFY(x) AE_STRINGIFY_IMPL(x)

// Ahamkara ABI version baked into this consumer.  When the installed package
// has a different major ABI version we must refuse to compile.
#ifndef EXPECTED_AE_ABI_VERSION
#define EXPECTED_AE_ABI_VERSION 1
#endif

// Wish ABI version baked into this consumer.
#ifndef EXPECTED_WISH_ABI_VERSION
#define EXPECTED_WISH_ABI_VERSION 1
#endif

// Wire protocol versions.
#ifndef EXPECTED_AE_NET_PROTOCOL_VERSION
#define EXPECTED_AE_NET_PROTOCOL_VERSION 1
#endif
#ifndef EXPECTED_WISH_SESSION_PROTOCOL_VERSION
#define EXPECTED_WISH_SESSION_PROTOCOL_VERSION 1
#endif

// Sanity-check that the installed headers match what we expect.
// In a "compatible artifact" test these all pass silently.
// In an "incompatible artifact" test we expect at least one to fire.
static_assert(ae::core::kAhamkaraAbiVersion == EXPECTED_AE_ABI_VERSION,
    "Ahamkara ABI version mismatch! "
    "Expected " AE_STRINGIFY(EXPECTED_AE_ABI_VERSION) " but installed headers report "
    "a different value. Verify that the Ahamkara package version matches.");

static_assert(wish::core::kWishAbiVersion == EXPECTED_WISH_ABI_VERSION,
    "Wish ABI version mismatch! "
    "Expected " AE_STRINGIFY(EXPECTED_WISH_ABI_VERSION) " but installed headers report "
    "a different value. Verify that the Wish package version matches.");

static_assert(ae::core::kAhamkaraNetProtocolVersion == EXPECTED_AE_NET_PROTOCOL_VERSION,
    "Ahamkara network protocol version mismatch! "
    "Expected " AE_STRINGIFY(EXPECTED_AE_NET_PROTOCOL_VERSION) " but installed headers "
    "report a different value. Peers will not be able to communicate.");

static_assert(wish::core::kWishSessionProtocolVersion == EXPECTED_WISH_SESSION_PROTOCOL_VERSION,
    "Wish session protocol version mismatch! "
    "Expected " AE_STRINGIFY(EXPECTED_WISH_SESSION_PROTOCOL_VERSION) " but installed "
    "headers report a different value. Peers will not be able to communicate.");

// ==========================================================================
// II. Runtime smoke — proves the installed libs actually link and run
// ==========================================================================

static int smoke_version_identity() {
    // Ahamkara identity
    ae::log_info_cat("compat_smoke",
        "Ahamkara ABI version: 1");
    ae::log_info_cat("compat_smoke",
        "Ahamkara net protocol: 1");

    // Wish identity
    const auto& wid = wish::core::identity();
    if (wid.name.empty() || wid.version.empty()) {
        ae::log_error_cat("compat_smoke", "Wish identity missing name or version");
        return 1;
    }
    ae::log_info_cat("compat_smoke",
        "Wish engine identity retrieved");

    // Verify the version strings match at runtime too
    if (std::strcmp(ae::core::kAhamkaraVersionString, "0.1.0") != 0) {
        ae::log_error_cat("compat_smoke", "Unexpected Ahamkara version string");
        return 1;
    }
    if (std::strcmp(wid.version.data(), wish::core::kWishVersionString) != 0) {
        ae::log_error_cat("compat_smoke", "Wish identity version mismatch with version.h");
        return 1;
    }

    return 0;
}

static int smoke_network_clock() {
    const double now = ae::now_seconds();
    ae::NetworkClock clock;
    clock.record_snapshot(1u, 60.0F, now);
    const double estimated = clock.estimate_server_time(now + 0.1, 60.0F);
    if (estimated < now) {
        ae::log_error_cat("compat_smoke", "Network clock estimate out of range");
        return 1;
    }
    return 0;
}

static int smoke_application_lifecycle() {
    ae::Application app(ae::RuntimeMode::Tests);
    if (!app.start()) {
        ae::log_error_cat("compat_smoke", "Application start failed");
        return 1;
    }
    app.shutdown();
    return 0;
}

// ==========================================================================
// III. Main — bounded smoke runner
// ==========================================================================

int main() {
    ae::log_info_cat("compat_smoke", "Cross-product compatibility consumer smoke start");

    int failures = 0;

    failures += smoke_version_identity();
    failures += smoke_network_clock();
    failures += smoke_application_lifecycle();

    if (failures == 0) {
        ae::log_info_cat("compat_smoke", "All compatibility smoke checks PASSED");
        std::printf("compatibility_consumer: ALL PASSED\n");
        return 0;
    } else {
        ae::log_error_cat("compat_smoke",
            "Compatibility smoke check(s) FAILED");
        std::printf("compatibility_consumer: %d FAILURE(S)\n", failures);
        return 1;
    }
}
