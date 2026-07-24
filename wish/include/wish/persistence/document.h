#pragma once

#include "wish/core/error_codes.h"
#include "wish/types.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wish::persistence {

// ---------------------------------------------------------------------------
// Limits
// ---------------------------------------------------------------------------

/// Maximum payload size for a single document (1 MiB).
inline constexpr std::size_t kMaxDocumentPayloadSize = 1 * 1024 * 1024;

/// Maximum number of documents in a single collection.
inline constexpr std::size_t kMaxDocumentsPerCollection = 10'000;

/// Maximum total storage per identity (100 MiB).
inline constexpr std::size_t kMaxStoragePerIdentity = 100 * 1024 * 1024;

// ---------------------------------------------------------------------------
// Version type
// ---------------------------------------------------------------------------

/// Monotonically increasing version counter for optimistic concurrency.
using DocumentVersion = std::uint64_t;

/// The version assigned to a newly created document.
inline constexpr DocumentVersion kInitialDocumentVersion = 1;

// ---------------------------------------------------------------------------
// DocumentId
// ---------------------------------------------------------------------------

/// Uniquely identifies a document within the persistence layer.
struct DocumentId {
    std::string collection;
    std::string key;

    bool operator==(const DocumentId& other) const {
        return collection == other.collection && key == other.key;
    }

    bool operator!=(const DocumentId& other) const {
        return !(*this == other);
    }
};

// ---------------------------------------------------------------------------
// PersistedDocument
// ---------------------------------------------------------------------------

/// An opaque document with metadata.  No game-specific types appear here.
struct PersistedDocument {
    DocumentId id;
    DocumentVersion version{kInitialDocumentVersion};
    std::string owner_id;
    std::vector<std::uint8_t> data;
    std::string content_type;
};

// ---------------------------------------------------------------------------
// WriteResult
// ---------------------------------------------------------------------------

/// Outcome of a write operation.
struct WriteResult {
    DocumentVersion version{kInitialDocumentVersion};
    bool created{false};
};

// ---------------------------------------------------------------------------
// PersistenceError
// ---------------------------------------------------------------------------

/// Structured error returned by persistence operations.
/// Maps directly to a WishErrorCode for wire transport.
struct PersistenceError {
    WishErrorCode code{WishErrorCode::kInternalError};
    std::string message;

    bool ok() const {
        return code == WishErrorCode::kInternalError && message.empty();
    }

    bool is_error() const {
        return code != WishErrorCode::kInternalError || !message.empty();
    }
};

/// Build a PersistenceError from a code and a message.
[[nodiscard]] inline PersistenceError make_persistence_error(
    WishErrorCode code,
    std::string message) {
    return PersistenceError{code, std::move(message)};
}

// ---------------------------------------------------------------------------
// PersistenceResult<T>
// ---------------------------------------------------------------------------

/// Result type for persistence operations.
template <typename T>
struct PersistenceResult {
    T value{};
    PersistenceError error{};
    bool ok{false};
};

} // namespace wish::persistence
