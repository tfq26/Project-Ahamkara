#pragma once

#include <string_view>

namespace ae {

// --- Basic logging (uncategorized, kept for backward compatibility) ---

void log_info(std::string_view message);
void log_warning(std::string_view message);
void log_error(std::string_view message);

// --- Structured logging with categories ---
//
// Usage:
//   #define AE_LOG_CATEGORY "Network"
//   log_info_cat(AE_LOG_CATEGORY, "Packet received");
// The category is prepended to the log line: [Info][Network][0.123] Packet received

void log_info_cat(std::string_view category, std::string_view message);
void log_warning_cat(std::string_view category, std::string_view message);
void log_error_cat(std::string_view category, std::string_view message);

}  // namespace ae

