#ifndef TRANSPORT_TCP_CLIENT_HPP
#define TRANSPORT_TCP_CLIENT_HPP

#include <cstdint>
#include <string>

class TcpClient {
private:
    std::string address;
    std::uint16_t port;
    int socketFd = -1;

public:
    TcpClient(const std::string& address, std::uint16_t port);
    ~TcpClient();
    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;
    TcpClient(TcpClient&&) = delete;
    TcpClient& operator=(TcpClient&&) = delete;

    void connect();
    // Sends bytes unchanged; callers supply LF delimiters. Failure disconnects.
    void send(const std::string& message);
    void disconnect() noexcept;
};

#endif // TRANSPORT_TCP_CLIENT_HPP
