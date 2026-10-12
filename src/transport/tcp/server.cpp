#include <transport/tcp/server.hpp>
#include <transport/detail/socket.hpp>
#include <transport/tcp/detail/framing.hpp>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <cerrno>
#include <stdexcept>
#include <system_error>

TcpServer::TcpServer(const std::string& address, std::uint16_t port)
    : address(address),
      port(port) {
    if (address.find('\0') != std::string::npos) {
        throw std::invalid_argument("Invalid TCP IPv4 address: embedded null character");
    }
}

void TcpServer::run(const std::function<void(const std::string&)>& onMessage) {
    if (!onMessage) {
        throw std::invalid_argument("TCP message callback is empty");
    }
    transport_detail::Socket listener(::socket(AF_INET, SOCK_STREAM, 0));
    if (listener.get() < 0) {
        throw std::system_error(errno, std::generic_category(), "TCP socket creation");
    }
    const int enabled = 1;
    if (::setsockopt(listener.get(), SOL_SOCKET, SO_REUSEADDR,
                     &enabled, sizeof(enabled)) < 0) {
        throw std::system_error(errno, std::generic_category(), "TCP SO_REUSEADDR");
    }
    sockaddr_in endpoint{};
    endpoint.sin_family = AF_INET;
    endpoint.sin_port = htons(port);
    if (::inet_pton(AF_INET, address.c_str(), &endpoint.sin_addr) != 1) {
        throw std::invalid_argument("Invalid TCP IPv4 address: " + address);
    }
    if (::bind(listener.get(), reinterpret_cast<sockaddr*>(&endpoint), sizeof(endpoint)) < 0) {
        throw std::system_error(errno, std::generic_category(), "TCP bind");
    }
    if (::listen(listener.get(), 1) < 0) {
        throw std::system_error(errno, std::generic_category(), "TCP listen");
    }
    int accepted;
    do {
        accepted = ::accept(listener.get(), nullptr, nullptr);
    } while (accepted < 0 && errno == EINTR);
    if (accepted < 0) {
        throw std::system_error(errno, std::generic_category(), "TCP accept");
    }
    transport_detail::Socket connection(accepted);
    tcp_detail::LineFramer framer;
    char buffer[1024];
    for (;;) {
        const ssize_t count = ::recv(connection.get(), buffer, sizeof(buffer), 0);
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::system_error(errno, std::generic_category(), "TCP receive");
        }
        if (count == 0) {
            framer.finish();
            return;
        }
        framer.consume(std::string_view(buffer, static_cast<std::size_t>(count)), onMessage);
    }
}
