#pragma once

#include "wish/persistence/document_store.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace wish::persistence {

// ---------------------------------------------------------------------------
// InMemoryDocumentStore — test backend / adapter seam
// ---------------------------------------------------------------------------
//
// Thread-safe in-memory implementation of DocumentStore.
// Used for unit testing and as a reference for production adapters.
//
// Does not persist anything across process boundaries.  Every method
// acquires a mutex so the store is safe to use from multiple threads
// in test scenarios.
//
// ---------------------------------------------------------------------------
class InMemoryDocumentStore final : public DocumentStore {
  public:
    InMemoryDocumentStore() = default;

    // Disable copy / move
    InMemoryDocumentStore(const InMemoryDocumentStore&) = delete;
    InMemoryDocumentStore& operator=(const InMemoryDocumentStore&) = delete;

    // ── DocumentStore overrides ────────────────────────────────────────────

    bool init() override;
    void shutdown() override;

    PersistenceResult<PersistedDocument> read(
        const std::string& identity,
        const DocumentId& id) override;

    PersistenceResult<WriteResult> write(
        const std::string& identity,
        const PersistedDocument& doc) override;

    PersistenceResult<bool> remove(
        const std::string& identity,
        const DocumentId& id) override;

    PersistenceResult<bool> exists(
        const std::string& identity,
        const DocumentId& id) override;

    PersistenceResult<std::size_t> storage_used(
        const std::string& identity) override;

    PersistenceResult<std::size_t> remove_all(
        const std::string& identity) override;

    // ── Test helpers ───────────────────────────────────────────────────────

    /// Returns the number of documents currently stored.
    std::size_t document_count() const;

    /// Simulate a backend failure — all mutating operations will fail until
    /// simulate_backend_recovered() is called.
    void simulate_backend_failure();

    /// Restore backend availability after simulate_backend_failure().
    void simulate_backend_recovered();

  private:
    struct StoredDocument {
        PersistedDocument doc;
    };

    /// Key = collection + "::" + key
    static std::string make_key(const DocumentId& id);

    mutable std::mutex mutex_;
    bool initialized_{false};
    bool backend_failed_{false};
    std::unordered_map<std::string, StoredDocument> store_;
};

} // namespace wish::persistence
