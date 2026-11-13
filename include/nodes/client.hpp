#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "node.hpp"
#include "network.hpp"
#include <transport/tcp/server.hpp>
#include <optional>
#include <cstdint>
#include <string>
#include <chrono>
#include <transport/udp/receiver.hpp>

class Client : public Node {
private:
    std::optional<TcpServer> server;
    std::optional<UdpReceiver> udpServer;
    std::chrono::milliseconds udpTimeout = UDP_SESSION_TIMEOUT;
    void runUdp();
    void runHttp();
    std::string httpAddress;
    std::uint16_t httpPort = 0;
    HttpSubmissionMode httpSubmission = HttpSubmissionMode::Combined;
    bool httpStayOpen = true;
    std::chrono::milliseconds httpTimeout = HTTP_SESSION_TIMEOUT;

public:
    Client() = default;
    Client(const std::string& bindAddress, std::uint16_t port,
           NodeTransport transport = NodeTransport::Tcp,
           std::chrono::milliseconds timeout = UDP_SESSION_TIMEOUT);
    Client(const std::string& bindAddress, std::uint16_t port,
           HttpSubmissionMode submission, std::chrono::milliseconds timeout = HTTP_SESSION_TIMEOUT,
           bool stayOpen = true);
    void run() override;
};

#endif // CLIENT_HPP
