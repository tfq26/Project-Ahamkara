#include "wish/core/match_runtime.h"

namespace wish::core {

static_assert(sizeof(session::SessionModel) > 0, "match runtime session state should stay lightweight");
static_assert(sizeof(replication::ReplicationFrame<int>) > 0, "match runtime replication frame should stay thin");

}  // namespace wish::core
