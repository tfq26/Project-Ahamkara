#pragma once

#include <cstdint>

namespace wish::net {

struct TransportConfig {
    std::uint16_t port = 0;
    std::uint32_t max_packet_size = 1200;
};

}  // namespace wish::net
