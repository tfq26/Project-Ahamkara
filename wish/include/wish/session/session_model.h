#pragma once

#include "wish/types.h"

#include <cstdint>

namespace wish::session {

struct SessionId {
    std::uint64_t value = 0;
};

struct SessionModel {
    SessionId id{};
    wish::NetAddress client_address {};
    bool connected = false;
    wish::SequenceTracker sequence_tracker {};
    wish::u32 server_tick {0};
    wish::u32 last_received_input_sequence {0};
    wish::u32 last_processed_input_sequence {0};
};

}  // namespace wish::session
