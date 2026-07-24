#define WISH_LOG_CATEGORY "ActivityManager"

#include "wish/core/activity_manager.h"
#include "wish/log.h"

namespace wish::core {

ActivityId ActivityManager::start_activity(ActivityId template_id,
                                            std::unique_ptr<IActivityBase> instance) {
    const ActivityConfig* tmpl = get_template(template_id);
    if (!tmpl) {
        wish::log_warning_cat(WISH_LOG_CATEGORY, "Unknown template id " + std::to_string(template_id));
        return 0;
    }

    if (!instance->initialize(*tmpl)) {
        wish::log_warning_cat(WISH_LOG_CATEGORY, "Failed to initialize activity from template " + std::string(tmpl->name));
        return 0;
    }

    ActivityEntry entry {};
    entry.instance = std::move(instance);
    entry.running = true;
    entries_.push_back(std::move(entry));

    wish::log_info_cat(WISH_LOG_CATEGORY, "Started activity '" + std::string(tmpl->name) + "' (id=" + std::to_string(tmpl->id) + ")");
    return tmpl->id;
}

ActivityId ActivityManager::start_activity(std::string_view name,
                                            std::unique_ptr<IActivityBase> instance) {
    const ActivityConfig* tmpl = get_template(name);
    if (!tmpl) {
        wish::log_warning_cat(WISH_LOG_CATEGORY, "Unknown template name '" + std::string(name) + "'");
        return 0;
    }
    return start_activity(tmpl->id, std::move(instance));
}

bool ActivityManager::route_packet(const wish::NetAddress& from,
                                   std::span<const std::byte> data,
                                   time_point now) {
    RoutingInfo* routing = find_routing(from);
    if (!routing) {
        wish::log_debug_cat(WISH_LOG_CATEGORY, "No routing found for address " + from.ip + ":" + std::to_string(from.port));
        return false;
    }

    IActivityBase* activity = get_activity(routing->activity_id);
    if (!activity) {
        wish::log_warning_cat(WISH_LOG_CATEGORY, "Routing points to unknown activity id " + std::to_string(routing->activity_id));
        return false;
    }

    // Activity-specific routing: the activity handles the packet
    // The server deserializes and calls activity methods directly.
    // This method is a light wrapper for address->activity lookup.
    wish::log_trace_cat(WISH_LOG_CATEGORY, "Routed packet to activity " + std::to_string(routing->activity_id));
    (void)data;
    (void)now;
    return true;
}

}  // namespace wish::core
