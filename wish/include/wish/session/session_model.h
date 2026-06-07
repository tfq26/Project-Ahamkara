#pragma once

#include "ae/network/sequence_tracker.h"
#include "ae/network/udp_socket.h"

#include <cstdint>

namespace wish::session {

struct SessionId {
    std::uint64_t value = 0;
};

struct SessionModel {
    SessionId id{};
    ae::NetAddress client_address {};
    bool connected = false;
    ae::SequenceTracker sequence_tracker {};
    ae::u32 server_tick {0};
    ae::u32 last_received_input_sequence {0};
    ae::u32 last_processed_input_sequence {0};
};

}  // namespace wish::session
