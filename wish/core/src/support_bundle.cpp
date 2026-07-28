#include "wish/core/support_bundle.h"

#include <algorithm>
#include <ctime>
#include <random>
#include <sstream>

namespace wish::core {

void OperationHistory::record_operation(OperationRecord record) {
    if (operations_.size() >= kMaxOperationHistory) {
        operations_.erase(operations_.begin());
    }
    operations_.push_back(std::move(record));
}

bool OperationHistory::has_operation_id(std::string_view operation_id) const {
    return find_operation(operation_id) != nullptr;
}

const OperationRecord* OperationHistory::find_operation(std::string_view operation_id) const {
    // Return the most recent entry (iterate in reverse)
    for (auto it = operations_.rbegin(); it != operations_.rend(); ++it) {
        if (it->operation_id == operation_id) {
            return &(*it);
        }
    }
    return nullptr;
}

bool OperationHistory::should_retry(std::string_view operation_id) const {
    const auto* record = find_operation(operation_id);
    if (!record) {
        return true;
    }

    if (record->outcome == OperationOutcome::Success) {
        return false;
    }

    if (record->outcome == OperationOutcome::Abandoned) {
        return false;
    }

    if (record->retry_count >= record->max_retries) {
        return false;
    }

    return true;
}

std::size_t OperationHistory::operation_count() const {
    return operations_.size();
}

const std::vector<OperationRecord>& OperationHistory::operations() const {
    return operations_;
}

void OperationHistory::clear() {
    operations_.clear();
}

std::string generate_bundle_id() {
    static const char kHexChars[] = "0123456789ABCDEF";
    static std::mt19937 rng(static_cast<unsigned>(
        std::chrono::system_clock::now().time_since_epoch().count()));

    char buffer[kMaxBundleIdLength + 1] {};
    buffer[0] = 'B';
    buffer[1] = 'N';
    buffer[2] = 'D';
    buffer[3] = '-';

    for (std::size_t i = 4; i < kMaxBundleIdLength; ++i) {
        buffer[i] = kHexChars[rng() % 16];
    }
    buffer[kMaxBundleIdLength] = '\0';

    return std::string(buffer);
}

SupportBundle build_support_bundle(
    const ServiceIdentity& service,
    std::string_view server_version,
    std::uint64_t uptime_seconds,
    const std::vector<CorrelatedFailure>& failures,
    const OperationHistory& history,
    std::string_view notes) {

    SupportBundle bundle {};
    bundle.bundle_id = generate_bundle_id();
    bundle.generated_at = std::chrono::system_clock::now();
    bundle.server_version = std::string(server_version);
    bundle.uptime_seconds = uptime_seconds;
    bundle.service = service;
    bundle.failures = failures;
    bundle.operations = history.operations();
    bundle.notes = std::string(notes);

    bundle.environment_keys = {
        "WISH_SERVER_PORT",
        "WISH_SERVER_ADMIN_PORT",
        "WISH_NAKAMA_ENABLED",
        "WISH_NAKAMA_HOST",
        "WISH_NAKAMA_PORT",
    };

    return bundle;
}

std::string format_operation_record(const OperationRecord& record) {
    std::ostringstream stream;

    stream << "op=" << record.operation_id
           << " name=" << record.operation_name
           << " outcome=";

    switch (record.outcome) {
        case OperationOutcome::Success:
            stream << "SUCCESS";
            break;
        case OperationOutcome::Failed:
            stream << "FAILED";
            break;
        case OperationOutcome::Retrying:
            stream << "RETRYING";
            break;
        case OperationOutcome::Abandoned:
            stream << "ABANDONED";
            break;
        case OperationOutcome::IdempotentReplay:
            stream << "IDEMPOTENT_REPLAY";
            break;
    }

    stream << " retries=" << record.retry_count
           << "/" << record.max_retries;

    if (record.error_code != 0) {
        stream << " error=" << record.error_code;
    }

    if (!record.diagnostic_message.empty()) {
        stream << " diag=" << record.diagnostic_message;
    }

    if (record.is_idempotent_replay) {
        stream << " [IDEMPOTENT]";
    }

    return stream.str();
}

std::string format_support_bundle(const SupportBundle& bundle) {
    std::ostringstream stream;

    stream << "=== Support Bundle " << bundle.bundle_id << " ===\n";
    stream << "Generated: " << std::chrono::system_clock::to_time_t(bundle.generated_at) << "\n";
    stream << "Service: " << bundle.service.service_name
           << " v" << bundle.service.version
           << " instance=" << bundle.service.instance_id << "\n";
    stream << "Server version: " << bundle.server_version << "\n";
    stream << "Uptime: " << bundle.uptime_seconds << "s\n";
    stream << "Active sessions: " << bundle.active_session_ids.size() << "\n";

    if (!bundle.failures.empty()) {
        stream << "\n-- Correlated Failures (" << bundle.failures.size() << ") --\n";
        for (const auto& f : bundle.failures) {
            stream << "  " << format_correlated_failure(f) << "\n";
        }
    }

    if (!bundle.operations.empty()) {
        stream << "\n-- Operation History (" << bundle.operations.size() << ") --\n";
        for (const auto& op : bundle.operations) {
            stream << "  " << format_operation_record(op) << "\n";
        }
    }

    if (!bundle.environment_keys.empty()) {
        stream << "\n-- Environment Keys --\n";
        for (const auto& key : bundle.environment_keys) {
            stream << "  " << key << "\n";
        }
    }

    if (!bundle.notes.empty()) {
        stream << "\n-- Notes --\n" << bundle.notes << "\n";
    }

    stream << "=== End Bundle ===";

    return stream.str();
}

} // namespace wish::core
