#ifndef PRIMARY_HPP
#define PRIMARY_HPP

#include "node.hpp"
#include "network.hpp"
#include <glove/pair.hpp>
#include <transport/tcp/client.hpp>
#include <transport/tcp/server.hpp>
#include <optional>
#include <cstdint>
#include <string>
#include <chrono>
#include <transport/udp/receiver.hpp>

class Primary : public Node {
private:
    GlovePair gloves;
    Hand localHand;
    std::optional<TcpServer> upstream;
    std::optional<TcpClient> connection;
    std::optional<UdpReceiver> udpUpstream;
    std::string udpPeer;
    std::uint16_t udpPort = 0;
    std::chrono::milliseconds udpTimeout = UDP_SESSION_TIMEOUT;
    std::string httpPeer;
    std::uint16_t httpPort = 0;
    HttpSubmissionMode httpSubmission = HttpSubmissionMode::Combined;
    void sendHttp(std::size_t index);
    void runHttp();
    void runLocal();
    void runTcp();
    void runUdp();

public:
    explicit Primary(Hand hand = Hand::Right);
    Primary(Hand hand, const std::string& peer, std::uint16_t port,
            NodeTransport transport = NodeTransport::Tcp,
            HttpSubmissionMode submission = HttpSubmissionMode::Combined);
    Primary(Hand hand, const std::string& bindAddress, std::uint16_t listenPort,
            const std::string& peer, std::uint16_t port,
            NodeTransport transport = NodeTransport::Tcp,
            std::chrono::milliseconds timeout = UDP_SESSION_TIMEOUT,
            std::optional<NodeTransport> inbound = std::nullopt);
    void run() override;
};

#endif // PRIMARY_HPP
