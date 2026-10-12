#ifndef SECONDARY_HPP
#define SECONDARY_HPP
#include "node.hpp"
#include "network.hpp"
#include <glove/glove.hpp>
#include <transport/tcp/client.hpp>
#include <optional>

class Secondary : public Node {
    Glove glove;
    std::optional<TcpClient> connection;
    bool udpMode = false;
    std::optional<BleConfig> bleMode;
    void runBle();
    std::string udpPeer;
    std::uint16_t udpPort = 0;
public:
    Secondary();
    Secondary(Hand hand, const BleConfig& config);
    Secondary(Hand hand, const std::string& peer, std::uint16_t port,
              NodeTransport transport = NodeTransport::Tcp);
    void run() override;
};

#endif // SECONDARY_HPP
