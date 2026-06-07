#include "wish/replication/replication_frame.h"

namespace wish::replication {

static_assert(sizeof(ReplicationFrame<int>) > 0, "replication frame should stay thin");

}  // namespace wish::replication
