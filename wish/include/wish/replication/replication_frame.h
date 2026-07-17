#pragma once

#include "wish/types.h"

#include <cstdint>

namespace wish::replication {

template <typename SnapshotPayload>
struct ReplicationFrame {
    wish::u32 server_tick {0};
    wish::u32 last_processed_input {0};
    bool authoritative {true};
    SnapshotPayload snapshot {};
};

}  // namespace wish::replication
