#include "ae/network/udp_socket.h"

#include "ae/core/log.h"

#define AE_LOG_CATEGORY "Network"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

namespace ae {
namespace {

bool set_non_blocking(int socket_fd) {
    const int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0) {
        log_error("Failed to read socket flags.");
        return false;
    }

    if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        log_error("Failed to set socket to non-blocking mode.");
        return false;
    }

    return true;
}

std::string make_errno_message(const char* prefix) {
    std::ostringstream stream;
    stream << prefix << ": " << std::strerror(errno);
    return stream.str();
}

}  // namespace

UdpSocket::~UdpSocket() {
    close();
}

bool UdpSocket::open(u16 port) {
    close();

    socket_fd_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_fd_ < 0) {
        log_error(make_errno_message("Failed to create UDP socket"));
        return false;
    }

    if (!set_non_blocking(socket_fd_)) {
        close();
        return false;
    }

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (::bind(socket_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        log_error(make_errno_message("Failed to bind UDP socket"));
        close();
        return false;
    }

    log_info_cat(AE_LOG_CATEGORY, "UDP socket opened on port " + std::to_string(port));
    return true;
}

void UdpSocket::close() {
    if (socket_fd_ >= 0) {
        log_info_cat(AE_LOG_CATEGORY, "UDP socket closed");
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
}

bool UdpSocket::send_to(const NetAddress& address, const void* data, usize size) const {
    if (!is_open()) {
        log_error("Attempted to send on a closed UDP socket.");
        return false;
    }

    sockaddr_in destination {};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(address.port);

    if (::inet_pton(AF_INET, address.ip.c_str(), &destination.sin_addr) != 1) {
        std::ostringstream stream;
        stream << "Failed to parse IP address '" << address.ip << "'.";
        log_error(stream.str());
        return false;
    }

    const auto sent = ::sendto(
        socket_fd_,
        data,
        size,
        0,
        reinterpret_cast<const sockaddr*>(&destination),
        sizeof(destination));

    if (sent < 0) {
        log_error(make_errno_message("Failed to send UDP packet"));
        return false;
    }

    if (static_cast<usize>(sent) != size) {
        log_warning_cat(AE_LOG_CATEGORY, "Partial send: " + std::to_string(sent) + "/" + std::to_string(static_cast<int>(size)) + " bytes");
        return false;
    }

    return true;
}

i32 UdpSocket::receive_from(NetAddress& from, void* buffer, usize max_size) const {
    if (!is_open()) {
        log_error("Attempted to receive on a closed UDP socket.");
        return -1;
    }

    sockaddr_in source {};
    socklen_t source_length = sizeof(source);
    const auto received = ::recvfrom(
        socket_fd_,
        buffer,
        max_size,
        0,
        reinterpret_cast<sockaddr*>(&source),
        &source_length);

    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }

        log_error(make_errno_message("Failed to receive UDP packet"));
        return -1;
    }

    char ip_buffer[INET_ADDRSTRLEN] {};
    if (::inet_ntop(AF_INET, &source.sin_addr, ip_buffer, sizeof(ip_buffer)) == nullptr) {
        log_error(make_errno_message("Failed to convert source address"));
        return -1;
    }

    from.ip = ip_buffer;
    from.port = ntohs(source.sin_port);
    return static_cast<i32>(received);
}

bool UdpSocket::is_open() const {
    return socket_fd_ >= 0;
}

}  // namespace ae

