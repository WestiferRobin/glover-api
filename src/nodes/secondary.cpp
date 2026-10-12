#include <nodes/secondary.hpp>
#include <mock.hpp>
#include <iostream>
#include <transport/detail/state_codec.hpp>
#include <transport/udp/detail/udp_envelope.hpp>
#include <transport/udp/sender.hpp>
#include <stdexcept>

Secondary::Secondary() : glove(Hand::Left, GloveRole::Secondary) {}

Secondary::Secondary(Hand hand, const std::string& peer, std::uint16_t port,
                     NodeTransport transport)
    : glove(hand, GloveRole::Secondary) {
    if (transport == NodeTransport::Tcp) connection.emplace(peer, port);
    else if (transport == NodeTransport::Udp) {
        udpPeer = peer;
        udpPort = port;
        udpMode = true;
    } else throw std::invalid_argument("Invalid node transport");
}

void Secondary::run() {
    if (bleMode) { runBle(); return; }
    if (udpMode) {
        UdpSender sender(udpPeer, udpPort);
        const auto& samples = mockSamples();
        for (std::size_t i = 0; i < samples.size(); ++i) {
            glove.setState(glove.getHand() == Hand::Left ? samples[i].left : samples[i].right);
            sender.send(serializeUdpSample(i, serializeHandState({glove.getHand(), glove.getState()})));
        }
        return;
    }
    if (!connection) {
        std::cout << "Secondary node started.\n";
        return;
    }
    try {
        connection->connect();
        for (const auto& sample : mockSamples()) {
            glove.setState(glove.getHand() == Hand::Left ? sample.left : sample.right);
            connection->send(serializeHandState({glove.getHand(), glove.getState()}) + '\n');
        }
        connection->disconnect();
    } catch (...) {
        connection->disconnect();
        throw;
    }
}
