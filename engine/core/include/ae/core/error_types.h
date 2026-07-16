#pragma once

#include "ae/core/error_code.h"

#include <array>
#include <memory>
#include <cstdint>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace ae {

enum class ErrorSeverity : std::uint8_t {
    Info = 0,
    Warning,
    Error,
    Fatal,
};

enum class RecoveryAction : std::uint8_t {
    None = 0,
    Retry,
    RestartSubsystem,
    RestartApp,
    ContactSupport,
};

struct RecoveryPolicy {
    RecoveryAction action {RecoveryAction::None};
    std::uint16_t max_retries {0};
    std::uint32_t retry_after_ms {0};
    bool user_visible {true};
};

/// Immutable catalog entry. Messages may change; codes must not.
struct ErrorDescriptor {
    ErrorCode code {};
    std::string_view title {};
    std::string_view message_key {};
    std::string_view owner_subsystem {};
    ErrorSeverity default_severity {ErrorSeverity::Error};
    RecoveryPolicy recovery {};
    std::string_view docs_path {};
};

/// Compact incident id for support correlation (e.g. 7F4A-19C2).
class IncidentId {
public:
    static constexpr std::size_t kTextSize = 9; // AAAA-BBBB

    IncidentId() = default;
    explicit IncidentId(std::uint64_t value);

    [[nodiscard]] static IncidentId generate();
    [[nodiscard]] std::string_view text() const { return text_; }
    [[nodiscard]] std::uint64_t value() const { return value_; }
    [[nodiscard]] bool empty() const { return value_ == 0; }

private:
    std::uint64_t value_ {0};
    char text_[kTextSize + 1] {"0000-0000"};
};

/// Small fixed context bag: up to N key/value pairs, no heap.
class SmallContext {
public:
    static constexpr std::size_t kMaxEntries = 8;
    static constexpr std::size_t kMaxKey = 24;
    static constexpr std::size_t kMaxValue = 48;

    struct Entry {
        char key[kMaxKey + 1] {};
        char value[kMaxValue + 1] {};
    };

    bool put(std::string_view key, std::string_view value);
    [[nodiscard]] std::size_t size() const { return size_; }
    [[nodiscard]] const Entry* begin() const { return entries_.data(); }
    [[nodiscard]] const Entry* end() const { return entries_.data() + size_; }
    [[nodiscard]] std::optional<std::string_view> get(std::string_view key) const;

private:
    std::array<Entry, kMaxEntries> entries_{};
    std::size_t size_ {0};
};

/// Runtime error instance with stable code + support metadata.
class Error {
public:
    Error() = default;
    explicit Error(ErrorCode code,
                   std::source_location loc = std::source_location::current());

    Error(ErrorCode code,
          IncidentId incident,
          SmallContext context,
          std::source_location loc = std::source_location::current());

    [[nodiscard]] bool ok() const { return !code_.valid(); }
    [[nodiscard]] const ErrorCode& code() const { return code_; }
    [[nodiscard]] const IncidentId& incident_id() const { return incident_; }
    [[nodiscard]] const SmallContext& context() const { return context_; }
    [[nodiscard]] const Error* cause() const { return cause_.get(); }
    [[nodiscard]] std::source_location source() const { return source_; }

    Error& with_context(std::string_view key, std::string_view value);
    Error& caused_by(Error cause);
    Error& with_native(std::string_view native_domain, std::int64_t native_code);

    [[nodiscard]] std::string_view native_domain() const { return native_domain_; }
    [[nodiscard]] std::int64_t native_code() const { return native_code_; }

private:
    ErrorCode code_{};
    IncidentId incident_{};
    SmallContext context_{};
    std::shared_ptr<Error> cause_{};
    std::source_location source_ {std::source_location::current()};
    char native_domain_[16] {};
    std::int64_t native_code_ {0};
};

/// Exception-free result type for Ahamkara boundaries.
template <typename T>
class Result {
public:
    Result(const T& value) : storage_(value) {}
    Result(T&& value) : storage_(std::move(value)) {}
    Result(Error error) : storage_(std::move(error)) {}

    [[nodiscard]] bool ok() const { return std::holds_alternative<T>(storage_); }
    [[nodiscard]] explicit operator bool() const { return ok(); }

    [[nodiscard]] T& value() & { return std::get<T>(storage_); }
    [[nodiscard]] const T& value() const& { return std::get<T>(storage_); }
    [[nodiscard]] T&& value() && { return std::get<T>(std::move(storage_)); }

    [[nodiscard]] Error& error() & { return std::get<Error>(storage_); }
    [[nodiscard]] const Error& error() const& { return std::get<Error>(storage_); }

private:
    std::variant<T, Error> storage_;
};

template <>
class Result<void> {
public:
    Result() : ok_(true) {}
    Result(Error error) : ok_(false), error_(std::move(error)) {}

    [[nodiscard]] bool ok() const { return ok_; }
    [[nodiscard]] explicit operator bool() const { return ok(); }
    [[nodiscard]] Error& error() { return error_; }
    [[nodiscard]] const Error& error() const { return error_; }

private:
    bool ok_ {true};
    Error error_{};
};

} // namespace ae
