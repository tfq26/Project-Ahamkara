#pragma once

#include "ae/core/types.h"

#include <cstdint>

namespace wish::replication {

template <typename SnapshotPayload>
struct ReplicationFrame {
    ae::u32 server_tick {0};
    ae::u32 last_processed_input {0};
    bool authoritative {true};
    SnapshotPayload snapshot {};
};

}  // namespace wish::replication
