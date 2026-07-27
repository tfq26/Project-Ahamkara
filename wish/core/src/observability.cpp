#include "wish/core/observability.h"
#include "wish/core/error_codes.h"
#include "wish/core/error_catalog.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace wish::core {

std::string generate_correlation_id(std::string_view seed) {
    std::uint32_t hash = 5381;
    for (const char c : seed) {
        hash = ((hash << 5) + hash) + static_cast<unsigned char>(c);
    }

    char buffer[20];
    std::snprintf(buffer, sizeof(buffer), "CORR-%08X", hash);
    return std::string(buffer);
}

CorrelatedFailure correlate_failure(
    WishErrorCode wish_code,
    std::uint32_t engine_error_code,
    std::string_view source_component,
    std::string_view message,
    bool recovered) {

    std::string seed(source_component);
    seed += ":";
    seed += std::to_string(static_cast<std::uint32_t>(wish_code));
    if (engine_error_code != 0) {
        seed += ":";
        seed += std::to_string(engine_error_code);
    }

    CorrelatedFailure failure {};
    failure.correlation_id = generate_correlation_id(seed);
    failure.wish_error_code = static_cast<std::uint32_t>(wish_code);
    failure.engine_error_code = engine_error_code;
    failure.message = std::string(message);
    failure.source_component = std::string(source_component);
    failure.recovered = recovered;

    return failure;
}

CorrelatedFailure correlate_envelope_failure(
    const ErrorEnvelope& envelope,
    std::uint32_t engine_error_code,
    std::string_view source_component,
    std::string_view message) {

    CorrelatedFailure failure {};
    failure.correlation_id = envelope.incident_id.empty()
        ? generate_correlation_id(source_component)
        : std::string("CORR-") + envelope.incident_id;
    failure.wish_error_code = envelope.error_code;
    failure.engine_error_code = engine_error_code;
    failure.message = std::string(message);
    failure.source_component = std::string(source_component);
    failure.recovered = false;

    return failure;
}

std::string format_correlated_failure(const CorrelatedFailure& failure) {
    std::ostringstream stream;
    stream << "[" << failure.correlation_id << "]"
           << " [" << failure.source_component << "]";

    char ws_code[12] {};
    if (failure.wish_error_code != 0) {
        format_wish_code(static_cast<WishErrorCode>(failure.wish_error_code), ws_code, sizeof(ws_code));
        stream << " WS=" << ws_code;
    }

    if (failure.engine_error_code != 0) {
        stream << " AE=AE-???"; // Engine codes are resolved elsewhere
        stream << "-" << std::setw(4) << std::setfill('0') << failure.engine_error_code;
    }

    stream << " " << failure.message;

    if (failure.recovered) {
        stream << " [RECOVERED]";
    }

    return stream.str();
}

std::string format_structured_event(const StructuredLogEvent& event) {
    std::ostringstream stream;

    stream << "{\"ts\":\"" << event.timestamp << "\""
           << ",\"level\":\"" << event.level << "\""
           << ",\"component\":\"" << event.component << "\"";

    if (!event.correlation_id.empty()) {
        stream << ",\"correlation_id\":\"" << event.correlation_id << "\"";
    }

    stream << ",\"message\":\"" << event.message << "\"";

    if (!event.metadata.empty()) {
        stream << ",\"metadata\":{";
        bool first = true;
        for (const auto& [key, value] : event.metadata) {
            if (!first) stream << ",";
            stream << "\"" << key << "\":\"" << value << "\"";
            first = false;
        }
        stream << "}";
    }

    stream << "}";
    return stream.str();
}

} // namespace wish::core
