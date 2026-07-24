#define WISH_LOG_CATEGORY "AdminCommand"

#include "wish/admin/admin_command.h"
#include "wish/log.h"

namespace wish::admin {

static_assert(sizeof(AdminCommand) > 0, "admin command should remain a simple descriptor");

namespace {
const bool kModuleLoaded = ([]() {
    wish::log_trace_cat(WISH_LOG_CATEGORY, "Admin command module loaded.");
    return true;
})();
} // anonymous namespace

}  // namespace wish::admin
