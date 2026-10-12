#include <transport/tcp/client.hpp>
#include <transport/detail/socket.hpp>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <cerrno>
#include <stdexcept>
#include <system_error>

TcpClient::TcpClient(const std::string& address, std::uint16_t port)
    : address(address),
      port(port) {
    if (address.find('\0') != std::string::npos) {
        throw std::invalid_argument("Invalid TCP IPv4 address: embedded null character");
    }
}

TcpClient::~TcpClient() {
    disconnect();
}

void TcpClient::connect() {
    if (socketFd >= 0) {
        throw std::logic_error("TCP client already connected");
    }
    transport_detail::Socket socket(::socket(AF_INET, SOCK_STREAM, 0));
    if (socket.get() < 0) {
        throw std::system_error(errno, std::generic_category(), "TCP socket creation");
    }
#if defined(SO_NOSIGPIPE)
    const int enabled = 1;
    if (::setsockopt(socket.get(), SOL_SOCKET, SO_NOSIGPIPE,
                     &enabled, sizeof(enabled)) < 0) {
        throw std::system_error(errno, std::generic_category(), "TCP SO_NOSIGPIPE");
    }
#elif !defined(MSG_NOSIGNAL)
#error "TCP requires SO_NOSIGPIPE or MSG_NOSIGNAL"
#endif
    sockaddr_in endpoint{};
    endpoint.sin_family = AF_INET;
    endpoint.sin_port = htons(port);
    if (::inet_pton(AF_INET, address.c_str(), &endpoint.sin_addr) != 1) {
        throw std::invalid_argument("Invalid TCP IPv4 address: " + address);
    }
    // A failed/interrupted connect is closed; callers may retry on a fresh socket.
    if (::connect(socket.get(), reinterpret_cast<sockaddr*>(&endpoint),
                  sizeof(endpoint)) < 0) {
        throw std::system_error(errno, std::generic_category(), "TCP connect");
    }
    socketFd = socket.release();
}

void TcpClient::send(const std::string& message) {
    if (socketFd < 0) {
        throw std::logic_error("TCP client is not connected");
    }
    std::size_t totalSent = 0;
    while (totalSent < message.size()) {
        int flags = 0;
#if defined(MSG_NOSIGNAL)
        flags = MSG_NOSIGNAL;
#endif
        const ssize_t sent = ::send(socketFd, message.data() + totalSent,
                                    message.size() - totalSent, flags);
        if (sent < 0) {
            const int error = errno;
            if (error == EINTR) {
                continue;
            }
            disconnect();
            throw std::system_error(error, std::generic_category(), "TCP send");
        }
        if (sent == 0) {
            disconnect();
            throw std::runtime_error("TCP send made no progress");
        }
        totalSent += static_cast<std::size_t>(sent);
    }
}

void TcpClient::disconnect() noexcept {
    if (socketFd >= 0) {
        ::close(socketFd);
        socketFd = -1;
    }
}
