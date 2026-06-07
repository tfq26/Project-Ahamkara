#pragma once

#include "ae/core/types.h"

#include <string>

namespace ae {

struct NetAddress {
    std::string ip {};
    u16 port {0};

    bool operator==(const NetAddress& other) const {
        return ip == other.ip && port == other.port;
    }

    bool operator!=(const NetAddress& other) const {
        return !(*this == other);
    }
};

class UdpSocket {
public:
    UdpSocket() = default;
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    UdpSocket(UdpSocket&&) = delete;
    UdpSocket& operator=(UdpSocket&&) = delete;
    ~UdpSocket();

    bool open(u16 port);
    void close();

    bool send_to(const NetAddress& address, const void* data, usize size) const;
    i32 receive_from(NetAddress& from, void* buffer, usize max_size) const;

    [[nodiscard]] bool is_open() const;

private:
    int socket_fd_ {-1};
};

}  // namespace ae

