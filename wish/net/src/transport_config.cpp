#define WISH_LOG_CATEGORY "TransportConfig"

#include "wish/net/transport_config.h"
#include "wish/log.h"

namespace wish::net {

static_assert(sizeof(TransportConfig) > 0, "transport config must remain trivially usable");

namespace {
const bool kModuleLoaded = ([]() {
    wish::log_trace_cat(WISH_LOG_CATEGORY, "Transport config module loaded.");
    return true;
})();
} // anonymous namespace

}  // namespace wish::net
