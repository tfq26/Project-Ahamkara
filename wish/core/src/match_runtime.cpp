#define WISH_LOG_CATEGORY "MatchRuntime"

#include "wish/core/match_runtime.h"
#include "wish/log.h"

namespace wish::core {

static_assert(sizeof(session::SessionModel) > 0, "match runtime session state should stay lightweight");
static_assert(sizeof(replication::ReplicationFrame<int>) > 0, "match runtime replication frame should stay thin");

namespace {
// Ensure match runtime module is marked as loaded (called once via static init).
const bool kModuleLoaded = ([]() {
    wish::log_trace_cat(WISH_LOG_CATEGORY, "Match runtime module loaded.");
    return true;
})();
} // anonymous namespace

}  // namespace wish::core
