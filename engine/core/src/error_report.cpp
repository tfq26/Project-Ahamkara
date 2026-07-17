#include "ae/core/error_report.h"

#include "ae/core/error_registry.h"
#include "ae/core/log.h"
#include "ae/core/telemetry.h"

#include <cctype>
#include <mutex>
#include <sstream>
#include <unordered_set>

namespace ae {
namespace {

std::mutex g_report_mu;
std::unordered_set<std::uint64_t> g_reported_incidents;

bool looks_sensitive_key(std::string_view key) {
    std::string lower(key);
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lower.find("token") != std::string::npos ||
           lower.find("password") != std::string::npos ||
           lower.find("secret") != std::string::npos ||
           lower.find("authorization") != std::string::npos ||
           lower.find("cookie") != std::string::npos ||
           lower.find("ip") != std::string::npos;
}

} // namespace

std::string redact_sensitive(std::string_view value) {
    // Redact anything that looks like a bearer/token-ish secret or IPv4.
    if (value.find("Bearer ") != std::string_view::npos ||
        value.find("eyJ") != std::string_view::npos) {
        return "<redacted>";
    }
    // crude IPv4 detect
    int dots = 0;
    bool has_digit = false;
    for (char c : value) {
        if (c == '.') {
            ++dots;
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            has_digit = true;
        } else if (c != ':') {
            has_digit = has_digit;
        }
    }
    if (dots == 3 && has_digit) {
        return "<redacted-ip>";
    }
    return std::string(value);
}

std::string format_player_error_banner(const Error& error) {
    std::ostringstream oss;
    oss << "Code: " << error.code().text() << "\n"
        << "Incident: " << error.incident_id().text();
    return oss.str();
}

ErrorReportResult report_error(const Error& error, ErrorReportOptions options) {
    ErrorReportResult out;
    if (!error.code().valid()) {
        return out;
    }

    out.incident_id = error.incident_id();
    if (out.incident_id.empty()) {
        out.incident_id = IncidentId::generate();
    }

    {
        std::lock_guard lock(g_report_mu);
        if (g_reported_incidents.contains(out.incident_id.value())) {
            out.reported = false;
            if (options.include_player_banner) {
                out.player_banner = format_player_error_banner(error);
            }
            return out;
        }
        g_reported_incidents.insert(out.incident_id.value());
    }

    const ErrorDescriptor* desc = ErrorRegistry::instance().find(error.code());
    const std::string_view title = desc ? desc->title : "Unknown error";
    const std::string_view message_key = desc ? desc->message_key : "error.unknown";

    if (options.emit_log) {
        std::ostringstream oss;
        oss << error.code().text() << " incident=" << out.incident_id.text()
            << " title=" << title << " message_key=" << message_key;
        if (!error.native_domain().empty()) {
            oss << " native=" << error.native_domain() << ":" << error.native_code();
        }
        for (const auto& entry : error.context()) {
            const auto key = std::string_view(entry.key);
            const auto value = looks_sensitive_key(key)
                                   ? std::string("<redacted>")
                                   : redact_sensitive(entry.value);
            oss << ' ' << key << '=' << value;
        }
        if (error.cause() != nullptr) {
            oss << " cause=" << error.cause()->code().text();
        }
        ae::log_error_cat("error", oss.str());
    }

    if (options.emit_telemetry) {
        // Metrics key by stable code only (never incident id).
        static ae::TelemetryCounter reported("error.reported");
        reported.add(1);
        // Keep code dimension in logs; counter itself remains code-agnostic to avoid label cardinality.
        ae::log_info_cat("telemetry", std::string("error.reported code=") + std::string(error.code().text()));
    }

    if (options.attach_crash_context) {
        // Keep crash breadcrumb free of secrets; code + incident only.
        ae::log_info_cat("crash_context",
                         std::string(error.code().text()) + " " + std::string(out.incident_id.text()));
    }

    if (options.include_player_banner) {
        out.player_banner = format_player_error_banner(error);
    }
    out.reported = true;
    return out;
}

} // namespace ae
