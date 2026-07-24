#include "wish/persistence/test_backend.h"
#include "wish/persistence/document.h"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <string>
#include <vector>

namespace wish::persistence {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::string InMemoryDocumentStore::make_key(const DocumentId& id) {
    return id.collection + "::" + id.key;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool InMemoryDocumentStore::init() {
    std::lock_guard lock(mutex_);
    initialized_ = true;
    return true;
}

void InMemoryDocumentStore::shutdown() {
    std::lock_guard lock(mutex_);
    initialized_ = false;
    store_.clear();
}

// ---------------------------------------------------------------------------
// Read
// ---------------------------------------------------------------------------

PersistenceResult<PersistedDocument> InMemoryDocumentStore::read(
    const std::string& identity,
    const DocumentId& id) {

    std::lock_guard lock(mutex_);

    if (!initialized_ || backend_failed_) {
        return PersistenceResult<PersistedDocument>{
            .error = make_persistence_error(
                WishErrorCode::kPersistenceBackendUnavailable,
                "Backend unavailable"),
            .ok = false};
    }

    const auto key = make_key(id);
    const auto it = store_.find(key);
    if (it == store_.end()) {
        return PersistenceResult<PersistedDocument>{
            .error = make_persistence_error(
                WishErrorCode::kPersistenceNotFound,
                "Document not found: " + id.collection + "/" + id.key),
            .ok = false};
    }

    const auto& stored = it->second.doc;
    if (stored.owner_id != identity) {
        return PersistenceResult<PersistedDocument>{
            .error = make_persistence_error(
                WishErrorCode::kPersistencePermissionDenied,
                "Identity does not own the document"),
            .ok = false};
    }

    return PersistenceResult<PersistedDocument>{
        .value = stored,
        .ok = true};
}

// ---------------------------------------------------------------------------
// Write
// ---------------------------------------------------------------------------

PersistenceResult<WriteResult> InMemoryDocumentStore::write(
    const std::string& identity,
    const PersistedDocument& doc) {

    std::lock_guard lock(mutex_);

    if (!initialized_ || backend_failed_) {
        return PersistenceResult<WriteResult>{
            .error = make_persistence_error(
                WishErrorCode::kPersistenceBackendUnavailable,
                "Backend unavailable"),
            .ok = false};
    }

    // Payload size limit
    if (doc.data.size() > kMaxDocumentPayloadSize) {
        return PersistenceResult<WriteResult>{
            .error = make_persistence_error(
                WishErrorCode::kPersistencePayloadTooLarge,
                "Payload exceeds maximum of " + std::to_string(kMaxDocumentPayloadSize) + " bytes"),
            .ok = false};
    }

    const auto key = make_key(doc.id);
    const auto it = store_.find(key);

    if (it == store_.end()) {
        // ── Create ─────────────────────────────────────────────────────────
        // Check identity storage quota
        std::size_t total = 0;
        for (const auto& [_, stored] : store_) {
            if (stored.doc.owner_id == identity) {
                total += stored.doc.data.size();
            }
        }
        if (total + doc.data.size() > kMaxStoragePerIdentity) {
            return PersistenceResult<WriteResult>{
                .error = make_persistence_error(
                    WishErrorCode::kPersistenceQuotaExceeded,
                    "Storage quota exceeded for identity"),
                .ok = false};
        }

        PersistedDocument new_doc = doc;
        new_doc.version = kInitialDocumentVersion;
        new_doc.owner_id = identity;

        store_[key] = StoredDocument{new_doc};

        return PersistenceResult<WriteResult>{
            .value = WriteResult{kInitialDocumentVersion, true},
            .ok = true};
    }

    // ── Update ─────────────────────────────────────────────────────────────
    auto& existing = it->second.doc;

    // Ownership check
    if (existing.owner_id != identity) {
        return PersistenceResult<WriteResult>{
            .error = make_persistence_error(
                WishErrorCode::kPersistencePermissionDenied,
                "Identity does not own the document"),
            .ok = false};
    }

    // Idempotent case: if version AND data match the stored document, silently
    // succeed without changing anything.  This prevents duplicate writes from
    // appearing as mutations.
    if (doc.version == existing.version && doc.data == existing.data) {
        return PersistenceResult<WriteResult>{
            .value = WriteResult{existing.version, false},
            .ok = true};
    }

    // Version mismatch → conflict (stale version or concurrent modification)
    if (doc.version != existing.version) {
        return PersistenceResult<WriteResult>{
            .error = make_persistence_error(
                WishErrorCode::kPersistenceConflict,
                "Version conflict: supplied " + std::to_string(doc.version) +
                    ", current " + std::to_string(existing.version)),
            .ok = false};
    }

    // Version matches, data differs — legitimate update
    existing.data = doc.data;
    existing.content_type = doc.content_type;
    existing.version++;

    return PersistenceResult<WriteResult>{
        .value = WriteResult{existing.version, false},
        .ok = true};
}

// ---------------------------------------------------------------------------
// Remove
// ---------------------------------------------------------------------------

PersistenceResult<bool> InMemoryDocumentStore::remove(
    const std::string& identity,
    const DocumentId& id) {

    std::lock_guard lock(mutex_);

    if (!initialized_ || backend_failed_) {
        return PersistenceResult<bool>{
            .error = make_persistence_error(
                WishErrorCode::kPersistenceBackendUnavailable,
                "Backend unavailable"),
            .ok = false};
    }

    const auto key = make_key(id);
    const auto it = store_.find(key);
    if (it == store_.end()) {
        return PersistenceResult<bool>{
            .value = false,
            .ok = true};
    }

    if (it->second.doc.owner_id != identity) {
        return PersistenceResult<bool>{
            .error = make_persistence_error(
                WishErrorCode::kPersistencePermissionDenied,
                "Identity does not own the document"),
            .ok = false};
    }

    store_.erase(it);
    return PersistenceResult<bool>{
        .value = true,
        .ok = true};
}

// ---------------------------------------------------------------------------
// Exists
// ---------------------------------------------------------------------------

PersistenceResult<bool> InMemoryDocumentStore::exists(
    const std::string& identity,
    const DocumentId& id) {

    std::lock_guard lock(mutex_);

    if (!initialized_ || backend_failed_) {
        return PersistenceResult<bool>{
            .error = make_persistence_error(
                WishErrorCode::kPersistenceBackendUnavailable,
                "Backend unavailable"),
            .ok = false};
    }

    const auto key = make_key(id);
    const auto it = store_.find(key);
    if (it == store_.end()) {
        return PersistenceResult<bool>{
            .value = false,
            .ok = true};
    }

    if (it->second.doc.owner_id != identity) {
        return PersistenceResult<bool>{
            .error = make_persistence_error(
                WishErrorCode::kPersistencePermissionDenied,
                "Identity does not own the document"),
            .ok = false};
    }

    return PersistenceResult<bool>{
        .value = true,
        .ok = true};
}

// ---------------------------------------------------------------------------
// Storage used
// ---------------------------------------------------------------------------

PersistenceResult<std::size_t> InMemoryDocumentStore::storage_used(
    const std::string& identity) {

    std::lock_guard lock(mutex_);

    if (!initialized_ || backend_failed_) {
        return PersistenceResult<std::size_t>{
            .error = make_persistence_error(
                WishErrorCode::kPersistenceBackendUnavailable,
                "Backend unavailable"),
            .ok = false};
    }

    std::size_t total = 0;
    for (const auto& [_, stored] : store_) {
        if (stored.doc.owner_id == identity) {
            total += stored.doc.data.size();
        }
    }

    return PersistenceResult<std::size_t>{
        .value = total,
        .ok = true};
}

// ---------------------------------------------------------------------------
// Remove all
// ---------------------------------------------------------------------------

PersistenceResult<std::size_t> InMemoryDocumentStore::remove_all(
    const std::string& identity) {

    std::lock_guard lock(mutex_);

    if (!initialized_ || backend_failed_) {
        return PersistenceResult<std::size_t>{
            .error = make_persistence_error(
                WishErrorCode::kPersistenceBackendUnavailable,
                "Backend unavailable"),
            .ok = false};
    }

    std::size_t removed = 0;
    for (auto it = store_.begin(); it != store_.end();) {
        if (it->second.doc.owner_id == identity) {
            it = store_.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }

    return PersistenceResult<std::size_t>{
        .value = removed,
        .ok = true};
}

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------

std::size_t InMemoryDocumentStore::document_count() const {
    std::lock_guard lock(mutex_);
    return store_.size();
}

void InMemoryDocumentStore::simulate_backend_failure() {
    std::lock_guard lock(mutex_);
    backend_failed_ = true;
}

void InMemoryDocumentStore::simulate_backend_recovered() {
    std::lock_guard lock(mutex_);
    backend_failed_ = false;
}

} // namespace wish::persistence
