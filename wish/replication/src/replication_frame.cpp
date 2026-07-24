#define WISH_LOG_CATEGORY "Replication"

#include "wish/replication/replication_frame.h"
#include "wish/log.h"

namespace wish::replication {

static_assert(sizeof(ReplicationFrame<int>) > 0, "replication frame should stay thin");

namespace {
const bool kModuleLoaded = ([]() {
    wish::log_trace_cat(WISH_LOG_CATEGORY, "Replication frame module loaded.");
    return true;
})();
} // anonymous namespace

}  // namespace wish::replication
