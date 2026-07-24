#pragma once

#include "wish/persistence/document.h"

#include <string>

namespace wish::persistence {

// ---------------------------------------------------------------------------
// DocumentStore — abstract persistence contract
// ---------------------------------------------------------------------------
//
// Game-neutral document storage interface.  The store knows nothing about
// Flashback rewards, inventory, pickups, characters, or reward tables.
//
// Every operation requires an authenticated identity string.  Ownership
// is enforced: a caller may only read/write/delete documents that belong
// to the supplied identity.
//
// Optimistic concurrency is enforced via DocumentVersion.  Writes will
// fail with kPersistenceConflict if the document has been modified since
// the caller last read it.
//
// Write operations are idempotent when the same version is supplied: if
// the document already exists at that exact version, the write succeeds
// without side effects and reports created=false.
//
// ---------------------------------------------------------------------------
class DocumentStore {
  public:
    virtual ~DocumentStore() = default;

    // ── Lifecycle ──────────────────────────────────────────────────────────

    /// Initialise the store.  Returns false if the backend cannot be reached.
    /// Calling any other method before init() succeeds is undefined behaviour.
    [[nodiscard]] virtual bool init() = 0;

    /// Gracefully shut down the store, flushing pending writes.
    virtual void shutdown() = 0;

    // ── Read ───────────────────────────────────────────────────────────────

    /// Retrieve a document by its id.
    /// The caller must own the document (identity must match owner_id).
    /// Returns kPersistenceNotFound if the document does not exist.
    /// Returns kPersistencePermissionDenied if identity does not match owner.
    [[nodiscard]] virtual PersistenceResult<PersistedDocument> read(
        const std::string& identity,
        const DocumentId& id) = 0;

    // ── Write ──────────────────────────────────────────────────────────────

    /// Create or update a document.
    ///
    /// **Create** – If no document exists for the given id, the store assigns
    /// kInitialDocumentVersion and returns created=true.
    ///
    /// **Update** – If the document already exists, the caller MUST supply the
    /// current version.  The store checks that:
    ///   - identity matches the existing owner_id (→ kPersistencePermissionDenied)
    ///   - the supplied version equals the stored version (→ kPersistenceConflict)
    ///
    /// **Idempotency** – Supplying the correct current version when the
    /// payload is unchanged returns created=false and the same version.
    ///
    /// **Size limits** – If data.size() > kMaxDocumentPayloadSize the
    /// operation fails with kPersistencePayloadTooLarge.  If the identity's
    /// total storage would exceed kMaxStoragePerIdentity the operation fails
    /// with kPersistenceQuotaExceeded.
    [[nodiscard]] virtual PersistenceResult<WriteResult> write(
        const std::string& identity,
        const PersistedDocument& doc) = 0;

    // ── Delete ─────────────────────────────────────────────────────────────

    /// Delete a document.
    /// Returns false (with ok=true) if the document did not exist.
    /// Returns kPersistencePermissionDenied if identity does not match owner.
    [[nodiscard]] virtual PersistenceResult<bool> remove(
        const std::string& identity,
        const DocumentId& id) = 0;

    // ── Existence check ────────────────────────────────────────────────────

    /// Check whether a document exists (without loading its payload).
    /// Returns kPersistencePermissionDenied if identity does not match owner.
    [[nodiscard]] virtual PersistenceResult<bool> exists(
        const std::string& identity,
        const DocumentId& id) = 0;

    // ── Administration ─────────────────────────────────────────────────────

    /// Return the total number of bytes stored for a given identity.
    [[nodiscard]] virtual PersistenceResult<std::size_t> storage_used(
        const std::string& identity) = 0;

    /// Remove all documents owned by identity.  Useful for account cleanup.
    [[nodiscard]] virtual PersistenceResult<std::size_t> remove_all(
        const std::string& identity) = 0;
};

} // namespace wish::persistence
