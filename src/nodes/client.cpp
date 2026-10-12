#include <nodes/client.hpp>
#include <iostream>
#include <transport/detail/state_codec.hpp>
#include <transport/udp/detail/udp_envelope.hpp>
#include <stdexcept>

Client::Client(const std::string& bindAddress, std::uint16_t port,
               NodeTransport transport, std::chrono::milliseconds timeout)
    : udpTimeout(timeout) {
    if (transport == NodeTransport::Tcp) {
        server.emplace(bindAddress, port);
    } else if (transport == NodeTransport::Udp) {
        udpServer.emplace(bindAddress, port);
    } else {
        throw std::invalid_argument("Invalid node transport");
    }
}

void Client::runUdp() {
    std::array<bool, UDP_SAMPLE_COUNT> seen{};
    std::array<std::array<bool, PAIR_FINGER_COUNT>, UDP_SAMPLE_COUNT> states{};
    try {
        udpServer->run(UDP_SAMPLE_COUNT, udpTimeout, [&](const std::string& datagram) {
            const auto sample = parseUdpSample(datagram);
            const auto state = parseState(sample.payload);
            if (seen[sample.index]) {
                throw std::invalid_argument(
                    "Duplicate UDP sample index: " + std::to_string(sample.index));
            }
            states[sample.index] = state;
            seen[sample.index] = true;
        });
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string(error.what()) + missingUdpSamples(seen));
    }
    // Publish only a complete valid session, in fixture order regardless of arrival order.
    for (const auto& state : states) {
        std::cout << serializeState(state) << '\n';
    }
}

void Client::run() {
    if (udpServer) {
        runUdp();
        return;
    }
    if (!server) {
        std::cout << "Client node started.\n";
        return;
    }
    server->run([](const std::string& payload) {
        const auto state = parseState(payload);
        std::cout << serializeState(state) << '\n';
    });
}
