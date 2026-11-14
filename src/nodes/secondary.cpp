#include <nodes/secondary.hpp>
#include <mock.hpp>
#include <codec/glove_json.hpp>
#include <transport/http/client.hpp>
#include <iostream>
#include <transport/detail/state_codec.hpp>
#include <transport/udp/detail/udp_envelope.hpp>
#include <transport/udp/sender.hpp>
#include <stdexcept>

Secondary::Secondary()
    : glove(Hand::Left, GloveRole::Secondary) {}

Secondary::Secondary(Hand hand, const std::string& peer, std::uint16_t port,
                     NodeTransport transport)
    : glove(hand, GloveRole::Secondary) {
    if (transport == NodeTransport::Tcp) {
        connection.emplace(peer, port);
    } else if (transport == NodeTransport::Udp) {
        udpPeer = peer;
        udpPort = port;
        udpMode = true;
    } else if (transport == NodeTransport::Http) {
        if (!port) throw std::invalid_argument("HTTP port must be nonzero");
        httpPeer = peer;
        httpPort = port;
    } else {
        throw std::invalid_argument("Invalid node transport");
    }
}

void Secondary::runUdp() {
    UdpSender sender(udpPeer, udpPort);
    const auto& samples = mockSamples();
    for (std::size_t i = 0; i < samples.size(); ++i) {
        glove.setState(glove.getHand() == Hand::Left ? samples[i].left : samples[i].right);
        sender.send(
            serializeUdpSample(i, serializeHandState({glove.getHand(), glove.getState()})));
    }
}

void Secondary::runTcp() {
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

void Secondary::runHttp() {
    const auto& samples = mockSamples();
    for (std::size_t index = 0; index < samples.size(); ++index) {
        try {
            glove.setState(glove.getHand() == Hand::Left ? samples[index].left : samples[index].right);
            HttpClient client(httpPeer, httpPort);
            const auto body = glove_json::individual(glove.getRole(), glove.getHand(), index, glove.getState());
            const auto response = client.post("/gloves", body, "application/json");
            if (response.status != 204)
                throw std::runtime_error("expected HTTP 204, received HTTP " + std::to_string(response.status));
        } catch (const std::exception& error) {
            throw std::runtime_error("HTTP Secondary sample " + std::to_string(index) + " to " +
                httpPeer + ':' + std::to_string(httpPort) + ": " + error.what());
        }
    }
}

void Secondary::run() {
    if (httpPort) {
        runHttp();
    } else if (udpMode) {
        runUdp();
    } else if (connection) {
        runTcp();
    } else {
        std::cout << "Secondary node started.\n";
    }
}
