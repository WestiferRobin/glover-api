#ifndef TRANSPORT_TCP_SERVER_HPP
#define TRANSPORT_TCP_SERVER_HPP

#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>

class TcpServer {
private:
    std::string address;
    std::uint16_t port;

public:
    // Payload bytes excluding LF. Empty frames are delivered as empty strings.
    static constexpr std::size_t MAX_MESSAGE_BYTES = 4096;
    TcpServer(const std::string& address, std::uint16_t port);

    // Blocks for one client, then returns at clean EOF. Callback references are
    // valid only during invocation. Framing/socket/callback errors propagate.
    void run(const std::function<void(const std::string&)>& onMessage);
};

#endif // TRANSPORT_TCP_SERVER_HPP
