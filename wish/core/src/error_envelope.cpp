#include "wish/core/error_envelope.h"
#include "wish/core/error_catalog.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>

namespace wish {
namespace {

// Serialization uses a simple line-oriented key:value format.
// Each field is on its own line, terminated by '\n'.
// The format is versioned by the first field: VERSION:<n>\n
//
// Fields:
//   VERSION:<n>
//   CODE:<n>
//   INCIDENT:<hex>
//   MKEY:<localization_key>
//   RETRY:<0|1>
//   RETRY_AFTER:<seconds>
//
// Unknown fields are silently ignored. Missing optional fields get defaults.

constexpr std::string_view kFieldVersion = "VERSION:";
constexpr std::string_view kFieldCode = "CODE:";
constexpr std::string_view kFieldIncident = "INCIDENT:";
constexpr std::string_view kFieldMessageKey = "MKEY:";
constexpr std::string_view kFieldRetryable = "RETRY:";
constexpr std::string_view kFieldRetryAfter = "RETRY_AFTER:";

bool parse_line(std::string_view line, std::string_view prefix, std::string& value) {
    if (line.substr(0, prefix.size()) != prefix) {
        return false;
    }
    value = std::string(line.substr(prefix.size()));
    return true;
}

bool parse_line(std::string_view line, std::string_view prefix, std::uint32_t& value) {
    std::string str_value;
    if (!parse_line(line, prefix, str_value)) {
        return false;
    }
    char* end = nullptr;
    auto parsed = static_cast<std::uint64_t>(std::strtoull(str_value.c_str(), &end, 10));
    if (end == str_value.c_str() || *end != '\0') {
        return false;
    }
    if (parsed > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    value = static_cast<std::uint32_t>(parsed);
    return true;
}

bool parse_line_bool(std::string_view line, std::string_view prefix, bool& value) {
    std::string str_value;
    if (!parse_line(line, prefix, str_value)) {
        return false;
    }
    value = (str_value == "1");
    return true;
}

} // anonymous namespace

bool ErrorEnvelope::valid() const {
    if (version != kErrorEnvelopeVersion) {
        return false;
    }
    if (error_code == 0) {
        return false;
    }
    if (incident_id.size() > kMaxIncidentId) {
        return false;
    }
    if (message_key.size() > kMaxMessageKey) {
        return false;
    }
    // Validate the error code is known
    return wish_code_domain(static_cast<WishErrorCode>(error_code)) != nullptr;
}

std::string serialize_envelope(const ErrorEnvelope& envelope) {
    if (!envelope.valid()) {
        return {};
    }

    std::ostringstream os;
    os << kFieldVersion << envelope.version << '\n';
    os << kFieldCode << envelope.error_code << '\n';
    os << kFieldIncident << envelope.incident_id << '\n';
    os << kFieldMessageKey << envelope.message_key << '\n';
    os << kFieldRetryable << (envelope.retryable ? "1" : "0") << '\n';
    os << kFieldRetryAfter << envelope.retry_after_seconds << '\n';
    return os.str();
}

std::optional<ErrorEnvelope> deserialize_envelope(std::string_view data) {
    ErrorEnvelope envelope;

    std::istringstream stream {std::string(data)};
    std::string line;
    bool has_version = false;
    bool has_code = false;

    while (std::getline(stream, line)) {
        // Trim trailing \r for cross-platform compatibility
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        std::uint32_t uint_val = 0;
        bool bool_val = false;
        std::string str_val;

        if (parse_line(line, kFieldVersion, uint_val)) {
            envelope.version = uint_val;
            has_version = true;
        } else if (parse_line(line, kFieldCode, uint_val)) {
            envelope.error_code = uint_val;
            has_code = true;
        } else if (parse_line(line, kFieldIncident, str_val)) {
            if (str_val.size() > kMaxIncidentId) {
                return std::nullopt;
            }
            envelope.incident_id = std::move(str_val);
        } else if (parse_line(line, kFieldMessageKey, str_val)) {
            if (str_val.size() > kMaxMessageKey) {
                return std::nullopt;
            }
            envelope.message_key = std::move(str_val);
        } else if (parse_line_bool(line, kFieldRetryable, bool_val)) {
            envelope.retryable = bool_val;
        } else if (parse_line(line, kFieldRetryAfter, uint_val)) {
            envelope.retry_after_seconds = uint_val;
        }
        // Unknown fields are silently ignored (forward compatibility)
    }

    if (!has_version || !has_code) {
        return std::nullopt;
    }

    if (!envelope.valid()) {
        return std::nullopt;
    }

    return envelope;
}

} // namespace wish
