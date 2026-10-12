#include <transport/udp/receiver.hpp>
#include <transport/udp/detail/socket.hpp>
#include <transport/detail/socket.hpp>
#include <sys/socket.h>
#include <poll.h>
#include <array>
#include <cerrno>
#include <climits>
#include <system_error>

UdpReceiver::UdpReceiver(const std::string& address, int localPort) {
    const auto endpoint = udp_detail::endpoint(address, localPort);
    localAddressNetworkOrder = endpoint.sin_addr.s_addr;
    portNetworkOrder = endpoint.sin_port;
}

void UdpReceiver::run(std::size_t count, std::chrono::milliseconds timeout,
                      const std::function<void(const std::string&)>& onMessage) {
    if (!onMessage || count == 0 || timeout.count() <= 0 || timeout.count() > INT_MAX) {
        throw std::invalid_argument(
            "UDP run requires a callback, positive count, and timeout 1..INT_MAX ms");
    }
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    transport_detail::Socket socket(::socket(AF_INET, SOCK_DGRAM, 0));
    if (socket.get() < 0) {
        throw std::system_error(errno, std::generic_category(), "UDP socket creation");
    }
    sockaddr_in endpoint{};
    endpoint.sin_family = AF_INET;
    endpoint.sin_addr.s_addr = localAddressNetworkOrder;
    endpoint.sin_port = portNetworkOrder;
    if (::bind(socket.get(), reinterpret_cast<const sockaddr*>(&endpoint), sizeof(endpoint)) < 0) {
        throw std::system_error(errno, std::generic_category(), "UDP bind");
    }
    std::array<char, MAX_MESSAGE_BYTES> buffer{};
    std::size_t received = 0;
    const auto timedOut = [&] {
        return std::runtime_error("UDP session incomplete: received " + std::to_string(received) +
                                  " of " + std::to_string(count) + " datagrams before timeout");
    };
    while (received < count) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            throw timedOut();
        }
        const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - now);
        pollfd ready{socket.get(), POLLIN, 0};
        const int result = ::poll(&ready, 1, static_cast<int>(remaining.count()));
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::system_error(errno, std::generic_category(), "UDP poll");
        }
        if (result == 0) {
            continue;
        }
        if (ready.revents & POLLNVAL) {
            throw std::system_error(EBADF, std::generic_category(), "UDP poll");
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            throw timedOut();
        }
        iovec part{buffer.data(), buffer.size()};
        msghdr message{};
        message.msg_iov = &part;
        message.msg_iovlen = 1;
        // Nonblocking receive avoids a stale readiness indication defeating the deadline.
        const ssize_t size = ::recvmsg(socket.get(), &message, MSG_DONTWAIT);
        if (size < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            throw std::system_error(errno, std::generic_category(), "UDP receive");
        }
        if ((message.msg_flags & MSG_TRUNC) || static_cast<std::size_t>(size) > buffer.size()) {
            throw std::length_error("UDP datagram truncated: payload exceeds 4096 bytes");
        }
        onMessage(std::string(buffer.data(), static_cast<std::size_t>(size)));
        ++received;
        if (std::chrono::steady_clock::now() >= deadline) {
            throw timedOut();
        }
    }
}
