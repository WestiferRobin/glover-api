#ifndef SECONDARY_HPP
#define SECONDARY_HPP

#include "node.hpp"
#include "network.hpp"
#include <glove/glove.hpp>
#include <transport/tcp/client.hpp>
#include <optional>
#include <cstdint>
#include <string>

class Secondary : public Node {
    Glove glove;
    std::optional<TcpClient> connection;
    bool udpMode = false;
    std::string udpPeer;
    std::uint16_t udpPort = 0;

    std::string httpPeer;
    std::uint16_t httpPort = 0;
    void runHttp();
    void runTcp();
    void runUdp();

public:
    Secondary();
    Secondary(Hand hand, const std::string& peer, std::uint16_t port,
              NodeTransport transport = NodeTransport::Tcp);
    void run() override;
};

#endif // SECONDARY_HPP
