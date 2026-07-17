#pragma once

#include "ae/core/error_types.h"

#include <string>

namespace ae {

struct ErrorReportOptions {
    bool emit_log {true};
    bool emit_telemetry {true};
    bool attach_crash_context {true};
    bool include_player_banner {true};
};

struct ErrorReportResult {
    IncidentId incident_id {};
    std::string player_banner {};
    bool reported {false};
};

/// Single reporting boundary for Ahamkara errors.
/// Fans out to logs/telemetry/crash context once per incident.
ErrorReportResult report_error(const Error& error, ErrorReportOptions options = {});

/// Format player-safe banner:
/// Code: AE-NET-0001
/// Incident: 7F4A-19C2
std::string format_player_error_banner(const Error& error);

/// Redact potentially sensitive values before context storage/reporting.
std::string redact_sensitive(std::string_view value);

} // namespace ae
