#pragma once

#include "wish/types.h"
#include "wish/log.h"
#include "wish/core/activity.h"

#include <string>
#include <string_view>
#include <vector>

namespace wish::core {

/// Minimal hand-rolled JSON parser for activity definitions.
/// Avoids pulling in a full JSON library; activity configs are simple enough.
struct ActivityLoader {
    /// Parse a single activity definition from a JSON string.
    /// Returns true on success, populates cfg. On failure, logs the error.
    static bool parse_one(std::string_view json, ActivityConfig& cfg);

    /// Parse an array of activity definitions.  Populates out_configs.
    /// Returns the number successfully parsed.
    static wish::u32 parse_many(std::string_view json,
                                std::vector<ActivityConfig>& out_configs);

    /// Load all activity definition files from a directory.
    /// Scans for *.json files and parses each.
    static wish::u32 load_directory(std::string_view path,
                                    std::vector<ActivityConfig>& out_configs);
};

}  // namespace wish::core
