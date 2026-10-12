#include <transport/udp/sender.hpp>
#include <transport/udp/detail/socket.hpp>
#include <transport/detail/socket.hpp>
#include <sys/socket.h>
#include <cerrno>
#include <system_error>

UdpSender::UdpSender(const std::string& address, int destinationPort) {
    const auto endpoint = udp_detail::endpoint(address, destinationPort);
    destinationNetworkOrder = endpoint.sin_addr.s_addr;
    portNetworkOrder = endpoint.sin_port;
    transport_detail::Socket socket(::socket(AF_INET, SOCK_DGRAM, 0));
    if (socket.get() < 0) {
        throw std::system_error(errno, std::generic_category(), "UDP socket creation");
    }
    socketFd = socket.release();
}

UdpSender::~UdpSender() {
    if (socketFd >= 0) {
        ::close(socketFd);
    }
}

void UdpSender::send(const std::string& message) {
    if (message.size() > MAX_MESSAGE_BYTES) {
        throw std::length_error("UDP payload exceeds 4096 bytes");
    }
    sockaddr_in endpoint{};
    endpoint.sin_family = AF_INET;
    endpoint.sin_addr.s_addr = destinationNetworkOrder;
    endpoint.sin_port = portNetworkOrder;
    ssize_t sent;
    do {
        sent = ::sendto(socketFd, message.data(), message.size(), 0,
                        reinterpret_cast<const sockaddr*>(&endpoint), sizeof(endpoint));
        // POSIX EINTR means interruption before any data was transmitted.
    } while (sent < 0 && errno == EINTR);
    if (sent < 0) {
        throw std::system_error(errno, std::generic_category(), "UDP send");
    }
    if (static_cast<std::size_t>(sent) != message.size()) {
        throw std::runtime_error("UDP short datagram send; not retried");
    }
}
