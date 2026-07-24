#define WISH_LOG_CATEGORY "SessionModel"

#include "wish/session/session_model.h"
#include "wish/log.h"

namespace wish::session {

static_assert(sizeof(SessionModel) > 0, "session model should stay lightweight");

namespace {
const bool kModuleLoaded = ([]() {
    wish::log_trace_cat(WISH_LOG_CATEGORY, "Session model module loaded.");
    return true;
})();
} // anonymous namespace

}  // namespace wish::session
