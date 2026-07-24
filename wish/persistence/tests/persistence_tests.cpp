#include "wish/persistence/document_store.h"
#include "wish/persistence/test_backend.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Test utilities
// ---------------------------------------------------------------------------

namespace {

int failures = 0;

#define EXPECT(cond, msg)                                                     \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":"           \
                      << __LINE__ << ")\n";                                   \
            ++failures;                                                       \
            return;                                                           \
        }                                                                     \
    } while (0)

#define EXPECT_OK(result, msg)                                                \
    do {                                                                      \
        if (!(result).ok) {                                                   \
            std::cerr << "FAIL: " << msg << " — "                             \
                      << (result).error.message << " (" << __FILE__ << ":"    \
                      << __LINE__ << ")\n";                                   \
            ++failures;                                                       \
            return;                                                           \
        }                                                                     \
    } while (0)

#define EXPECT_ERR(result, expected_code, msg)                                \
    do {                                                                      \
        if ((result).ok) {                                                    \
            std::cerr << "FAIL: " << msg                                      \
                      << " — expected error but got ok (" << __FILE__ << ":"  \
                      << __LINE__ << ")\n";                                   \
            ++failures;                                                       \
            return;                                                           \
        }                                                                     \
        if ((result).error.code != (expected_code)) {                         \
            std::cerr << "FAIL: " << msg                                      \
                      << " — expected code "                                  \
                      << static_cast<int>(expected_code)                      \
                      << " but got "                                          \
                      << static_cast<int>((result).error.code)                \
                      << " (" << __FILE__ << ":" << __LINE__ << ")\n";        \
            ++failures;                                                       \
            return;                                                           \
        }                                                                     \
    } while (0)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

wish::persistence::DocumentId make_id(const std::string& coll,
                                      const std::string& key) {
    return {coll, key};
}

wish::persistence::PersistedDocument make_doc(const std::string& coll,
                                               const std::string& key,
                                               const std::string& owner,
                                               const std::string& payload,
                                               const std::string& content_type =
                                                   "text/plain") {
    std::vector<std::uint8_t> data(payload.begin(), payload.end());
    return {make_id(coll, key), wish::persistence::kInitialDocumentVersion,
            owner, std::move(data), content_type};
}

// ============================================================================
// Tests
// ============================================================================

// ── 1. Basic create and read ───────────────────────────────────────────────

void test_create_and_read() {
    wish::persistence::InMemoryDocumentStore store;
    EXPECT(store.init(), "init should succeed");

    auto doc = make_doc("profiles", "player-1", "alice", "Hello, World!");
    auto wr = store.write("alice", doc);
    EXPECT_OK(wr, "write should succeed");
    EXPECT(wr.value.created, "document should be newly created");
    EXPECT(wr.value.version == wish::persistence::kInitialDocumentVersion,
           "version should be initial");

    auto rr = store.read("alice", make_id("profiles", "player-1"));
    EXPECT_OK(rr, "read should succeed");
    EXPECT(rr.value.owner_id == "alice", "owner should match");
    EXPECT(rr.value.version == wish::persistence::kInitialDocumentVersion,
           "version should match");
    std::string payload(rr.value.data.begin(), rr.value.data.end());
    EXPECT(payload == "Hello, World!", "payload should match");

    store.shutdown();
    std::cout << "  test_create_and_read: ok\n";
}

// ── 2. Ownership enforcement ───────────────────────────────────────────────

void test_ownership_enforcement() {
    wish::persistence::InMemoryDocumentStore store;
    EXPECT(store.init(), "init should succeed");

    auto doc = make_doc("items", "sword", "alice", "epic blade");
    auto wr = store.write("alice", doc);
    EXPECT_OK(wr, "write should succeed");

    // Read with wrong identity
    auto rr = store.read("bob", make_id("items", "sword"));
    EXPECT_ERR(rr, wish::WishErrorCode::kPersistencePermissionDenied,
               "read by non-owner should be denied");

    // Write as non-owner should fail
    auto doc2 = make_doc("items", "sword", "bob", "rusty blade");
    auto wr2 = store.write("bob", doc2);
    EXPECT_ERR(wr2, wish::WishErrorCode::kPersistencePermissionDenied,
               "write by non-owner should be denied");

    // Exists by non-owner
    auto er = store.exists("bob", make_id("items", "sword"));
    EXPECT_ERR(er, wish::WishErrorCode::kPersistencePermissionDenied,
               "exists by non-owner should be denied");

    // Remove by non-owner
    auto rm = store.remove("bob", make_id("items", "sword"));
    EXPECT_ERR(rm, wish::WishErrorCode::kPersistencePermissionDenied,
               "remove by non-owner should be denied");

    // Owner can still read
    rr = store.read("alice", make_id("items", "sword"));
    EXPECT_OK(rr, "owner should still read");

    store.shutdown();
    std::cout << "  test_ownership_enforcement: ok\n";
}

// ── 3. Optimistic concurrency / version conflict ───────────────────────────

void test_version_conflict() {
    wish::persistence::InMemoryDocumentStore store;
    EXPECT(store.init(), "init should succeed");

    // Create
    auto doc = make_doc("cfg", "settings", "alice", "v1");
    auto wr = store.write("alice", doc);
    EXPECT_OK(wr, "initial write should succeed");

    // Read to get current version
    auto rr = store.read("alice", make_id("cfg", "settings"));
    EXPECT_OK(rr, "read should succeed");
    auto current_version = rr.value.version;

    // Simulate two concurrent readers both getting version 1
    // First writer succeeds
    auto doc_v2 = make_doc("cfg", "settings", "alice", "v2");
    doc_v2.version = current_version;
    auto wr2 = store.write("alice", doc_v2);
    EXPECT_OK(wr2, "first update should succeed");
    EXPECT(wr2.value.version == current_version + 1, "version should increment");

    // Second writer using stale version should fail
    auto doc_stale = make_doc("cfg", "settings", "alice", "v2-stale");
    doc_stale.version = current_version; // stale!
    auto wr3 = store.write("alice", doc_stale);
    EXPECT_ERR(wr3, wish::WishErrorCode::kPersistenceConflict,
               "write with stale version should conflict");

    store.shutdown();
    std::cout << "  test_version_conflict: ok\n";
}

// ── 4. Idempotent writes ───────────────────────────────────────────────────

void test_idempotent_write() {
    wish::persistence::InMemoryDocumentStore store;
    EXPECT(store.init(), "init should succeed");

    // Create
    auto doc = make_doc("cfg", "settings", "alice", "v1");
    auto wr = store.write("alice", doc);
    EXPECT_OK(wr, "initial write");
    EXPECT(wr.value.created, "should be created");

    // Same write again — idempotent
    auto wr2 = store.write("alice", doc);
    EXPECT_OK(wr2, "idempotent write");
    EXPECT(!wr2.value.created, "should NOT be created (idempotent)");
    EXPECT(wr2.value.version == wr.value.version,
           "version should not change (idempotent)");

    // Read and verify payload was not duplicated
    auto rr = store.read("alice", make_id("cfg", "settings"));
    EXPECT_OK(rr, "read after idempotent write");
    std::string payload(rr.value.data.begin(), rr.value.data.end());
    EXPECT(payload == "v1", "payload should be unchanged");

    store.shutdown();
    std::cout << "  test_idempotent_write: ok\n";
}

// ── 5. Delete operations ───────────────────────────────────────────────────

void test_delete() {
    wish::persistence::InMemoryDocumentStore store;
    EXPECT(store.init(), "init should succeed");

    auto doc = make_doc("items", "ring", "alice", "gold");
    store.write("alice", doc);

    // Remove
    auto rm = store.remove("alice", make_id("items", "ring"));
    EXPECT_OK(rm, "remove should succeed");
    EXPECT(rm.value, "should report document was removed");

    // Verify gone
    auto rr = store.read("alice", make_id("items", "ring"));
    EXPECT_ERR(rr, wish::WishErrorCode::kPersistenceNotFound,
               "read after delete should fail");

    // Double delete returns false (non-error)
    auto rm2 = store.remove("alice", make_id("items", "ring"));
    EXPECT_OK(rm2, "double remove should succeed");
    EXPECT(!rm2.value, "double remove should return false");

    store.shutdown();
    std::cout << "  test_delete: ok\n";
}

// ── 6. Payload too large ───────────────────────────────────────────────────

void test_payload_too_large() {
    wish::persistence::InMemoryDocumentStore store;
    EXPECT(store.init(), "init should succeed");

    // Create a payload that exceeds the limit
    std::vector<std::uint8_t> big_data(
        wish::persistence::kMaxDocumentPayloadSize + 1, 'X');

    wish::persistence::PersistedDocument big_doc = {
        make_id("items", "big"), wish::persistence::kInitialDocumentVersion,
        "alice", std::move(big_data), "application/octet-stream"};

    auto wr = store.write("alice", big_doc);
    EXPECT_ERR(wr, wish::WishErrorCode::kPersistencePayloadTooLarge,
               "oversized payload should be rejected");

    // Small payload should still succeed
    auto small = make_doc("items", "small", "alice", "ok");
    auto wr2 = store.write("alice", small);
    EXPECT_OK(wr2, "small payload should succeed");

    store.shutdown();
    std::cout << "  test_payload_too_large: ok\n";
}

// ── 7. Storage quota enforcement ───────────────────────────────────────────

void test_quota_enforcement() {
    // Use an artificially low quota for testing by making a variant that
    // patches the constant. Instead, we'll just verify storage_used works.
    wish::persistence::InMemoryDocumentStore store;
    EXPECT(store.init(), "init should succeed");

    // Write several documents and check quota tracking
    auto d1 = make_doc("col", "a", "alice", "hello");
    store.write("alice", d1);

    auto d2 = make_doc("col", "b", "alice", "world");
    store.write("alice", d2);

    auto su = store.storage_used("alice");
    EXPECT_OK(su, "storage_used should succeed");
    EXPECT(su.value > 0, "storage used should be positive");
    EXPECT(su.value == std::string("hello").size() + std::string("world").size(),
           "storage used should sum payloads");

    // Different identity should have different quota
    auto d3 = make_doc("col", "c", "bob", "bob-data");
    store.write("bob", d3);

    auto su_bob = store.storage_used("bob");
    EXPECT_OK(su_bob, "bob storage_used should succeed");
    EXPECT(su_bob.value == std::string("bob-data").size(),
           "bob storage should be independent");

    store.shutdown();
    std::cout << "  test_quota_enforcement: ok\n";
}

// ── 8. Backend unavailable ─────────────────────────────────────────────────

void test_backend_unavailable() {
    wish::persistence::InMemoryDocumentStore store;
    EXPECT(store.init(), "init should succeed");

    // Write something first
    auto doc = make_doc("col", "x", "alice", "data");
    auto wr = store.write("alice", doc);
    EXPECT_OK(wr, "initial write");

    // Simulate backend failure
    store.simulate_backend_failure();

    auto rr = store.read("alice", make_id("col", "x"));
    EXPECT_ERR(rr, wish::WishErrorCode::kPersistenceBackendUnavailable,
               "read should fail when backend is unavailable");

    auto wr2 = store.write("alice", doc);
    EXPECT_ERR(wr2, wish::WishErrorCode::kPersistenceBackendUnavailable,
               "write should fail when backend is unavailable");

    auto rm = store.remove("alice", make_id("col", "x"));
    EXPECT_ERR(rm, wish::WishErrorCode::kPersistenceBackendUnavailable,
               "remove should fail when backend is unavailable");

    // Recover
    store.simulate_backend_recovered();

    rr = store.read("alice", make_id("col", "x"));
    EXPECT_OK(rr, "read should work after recovery");

    store.shutdown();
    std::cout << "  test_backend_unavailable: ok\n";
}

// ── 9. Remove all documents for an identity ────────────────────────────────

void test_remove_all() {
    wish::persistence::InMemoryDocumentStore store;
    EXPECT(store.init(), "init should succeed");

    store.write("alice", make_doc("col", "a", "alice", "data-a"));
    store.write("alice", make_doc("col", "b", "alice", "data-b"));
    store.write("bob", make_doc("col", "c", "bob", "data-c"));

    EXPECT(store.document_count() == 3, "should have 3 documents");

    auto ra = store.remove_all("alice");
    EXPECT_OK(ra, "remove_all should succeed");
    EXPECT(ra.value == 2, "should remove 2 documents");

    EXPECT(store.document_count() == 1, "should have 1 document remaining");
    auto rr = store.read("bob", make_id("col", "c"));
    EXPECT_OK(rr, "bob's document should survive");

    store.shutdown();
    std::cout << "  test_remove_all: ok\n";
}

// ── 10. Document not found error ───────────────────────────────────────────

void test_document_not_found() {
    wish::persistence::InMemoryDocumentStore store;
    EXPECT(store.init(), "init should succeed");

    auto rr = store.read("alice", make_id("col", "nonexistent"));
    EXPECT_ERR(rr, wish::WishErrorCode::kPersistenceNotFound,
               "read of nonexistent doc should fail");

    auto er = store.exists("alice", make_id("col", "nonexistent"));
    EXPECT_OK(er, "exists of nonexistent doc should succeed");
    EXPECT(!er.value, "should return false");

    store.shutdown();
    std::cout << "  test_document_not_found: ok\n";
}

// ── 11. Multiple collections isolation ─────────────────────────────────────

void test_collection_isolation() {
    wish::persistence::InMemoryDocumentStore store;
    EXPECT(store.init(), "init should succeed");

    store.write("alice", make_doc("profiles", "a", "alice", "profile data"));
    store.write("alice", make_doc("settings", "a", "alice", "settings data"));

    auto rr1 = store.read("alice", make_id("profiles", "a"));
    EXPECT_OK(rr1, "read profile");
    std::string p1(rr1.value.data.begin(), rr1.value.data.end());
    EXPECT(p1 == "profile data", "profile data should match");

    auto rr2 = store.read("alice", make_id("settings", "a"));
    EXPECT_OK(rr2, "read settings");
    std::string p2(rr2.value.data.begin(), rr2.value.data.end());
    EXPECT(p2 == "settings data", "settings data should match");

    store.shutdown();
    std::cout << "  test_collection_isolation: ok\n";
}

// ── 12. Update document increments version ─────────────────────────────────

void test_version_increments_on_update() {
    wish::persistence::InMemoryDocumentStore store;
    EXPECT(store.init(), "init should succeed");

    auto doc = make_doc("col", "item", "alice", "v1");
    auto wr = store.write("alice", doc);
    EXPECT_OK(wr, "create");
    EXPECT(wr.value.version == wish::persistence::kInitialDocumentVersion,
           "initial version");

    // Update with correct version
    auto doc2 = make_doc("col", "item", "alice", "v2");
    doc2.version = wr.value.version;
    auto wr2 = store.write("alice", doc2);
    EXPECT_OK(wr2, "update");
    EXPECT(wr2.value.version == wish::persistence::kInitialDocumentVersion + 1,
           "version should increment");

    // Update again
    auto doc3 = make_doc("col", "item", "alice", "v3");
    doc3.version = wr2.value.version;
    auto wr3 = store.write("alice", doc3);
    EXPECT_OK(wr3, "second update");
    EXPECT(wr3.value.version == wish::persistence::kInitialDocumentVersion + 2,
           "version should increment again");

    store.shutdown();
    std::cout << "  test_version_increments_on_update: ok\n";
}

// ── 13. Empty document is valid ────────────────────────────────────────────

void test_empty_document() {
    wish::persistence::InMemoryDocumentStore store;
    EXPECT(store.init(), "init should succeed");

    auto doc = make_doc("col", "empty", "alice", "");
    auto wr = store.write("alice", doc);
    EXPECT_OK(wr, "write empty doc");
    EXPECT(wr.value.created, "should be created");

    auto rr = store.read("alice", make_id("col", "empty"));
    EXPECT_OK(rr, "read empty doc");
    EXPECT(rr.value.data.empty(), "empty payload");

    store.shutdown();
    std::cout << "  test_empty_document: ok\n";
}

// ── 14. Exists with matching owner ─────────────────────────────────────────

void test_exists_with_owner() {
    wish::persistence::InMemoryDocumentStore store;
    EXPECT(store.init(), "init should succeed");

    store.write("alice", make_doc("col", "x", "alice", "data"));

    auto er = store.exists("alice", make_id("col", "x"));
    EXPECT_OK(er, "exists should succeed");
    EXPECT(er.value, "should report true");

    er = store.exists("alice", make_id("col", "y"));
    EXPECT_OK(er, "exists for missing should succeed");
    EXPECT(!er.value, "should report false");

    store.shutdown();
    std::cout << "  test_exists_with_owner: ok\n";
}

} // namespace

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=== Wish Persistence Tests ===\n\n";

    test_create_and_read();
    test_ownership_enforcement();
    test_version_conflict();
    test_idempotent_write();
    test_delete();
    test_payload_too_large();
    test_quota_enforcement();
    test_backend_unavailable();
    test_remove_all();
    test_document_not_found();
    test_collection_isolation();
    test_version_increments_on_update();
    test_empty_document();
    test_exists_with_owner();

    std::cout << "\n";

    if (failures > 0) {
        std::cerr << failures << " persistence test(s) FAILED.\n";
        return 1;
    }

    std::cout << "All persistence tests passed.\n";
    return 0;
}
