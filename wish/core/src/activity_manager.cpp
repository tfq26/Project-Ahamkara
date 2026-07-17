#include "wish/core/activity_manager.h"
#include "wish/log.h"

namespace wish::core {

ActivityId ActivityManager::start_activity(ActivityId template_id,
                                            std::unique_ptr<IActivityBase> instance) {
    const ActivityConfig* tmpl = get_template(template_id);
    if (!tmpl) {
        wish::log_warning("ActivityManager: unknown template id " + std::to_string(template_id));
        return 0;
    }

    if (!instance->initialize(*tmpl)) {
        wish::log_warning("ActivityManager: failed to initialize activity from template " + std::string(tmpl->name));
        return 0;
    }

    ActivityEntry entry {};
    entry.instance = std::move(instance);
    entry.running = true;
    entries_.push_back(std::move(entry));

    wish::log_info("ActivityManager: started activity '" + std::string(tmpl->name) + "' (id=" + std::to_string(tmpl->id) + ")");
    return tmpl->id;
}

ActivityId ActivityManager::start_activity(std::string_view name,
                                            std::unique_ptr<IActivityBase> instance) {
    const ActivityConfig* tmpl = get_template(name);
    if (!tmpl) {
        wish::log_warning("ActivityManager: unknown template name '" + std::string(name) + "'");
        return 0;
    }
    return start_activity(tmpl->id, std::move(instance));
}

bool ActivityManager::route_packet(const wish::NetAddress& from,
                                   std::span<const std::byte> data,
                                   time_point now) {
    RoutingInfo* routing = find_routing(from);
    if (!routing) return false;

    IActivityBase* activity = get_activity(routing->activity_id);
    if (!activity) return false;

    // Activity-specific routing: the activity handles the packet
    // The server deserializes and calls activity methods directly.
    // This method is a light wrapper for address->activity lookup.
    (void)data;
    (void)now;
    return true;
}

}  // namespace wish::core
