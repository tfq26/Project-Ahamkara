#include "wish/integrations/flashback/game_session_adapter.h"

namespace wish::integrations::flashback {

MatchReport build_match_report(
    wish::core::ActivityId activity_id,
    std::string_view activity_name,
    float duration_seconds,
    const std::vector<std::string>& participant_ids,
    bool was_completed,
    std::string_view summary) {

    MatchReport report {};
    report.activity_id = activity_id;
    report.activity_name = std::string(activity_name);
    report.duration_seconds = duration_seconds;
    report.participant_ids = participant_ids;
    report.was_completed = was_completed;
    report.summary = std::string(summary);
    return report;
}

} // namespace wish::integrations::flashback
