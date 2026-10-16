#include <nodes/primary.hpp>
#include <mock.hpp>
#include <iostream>
#include <stdexcept>
#include <transport/detail/state_codec.hpp>
#include <transport/udp/detail/udp_envelope.hpp>
#include <transport/udp/sender.hpp>

namespace {
GlovePair makePair(Hand hand) {
    // Glove validates the configured hand before selecting its opposite.
    Glove local(hand, GloveRole::Primary);
    Glove remote(hand == Hand::Left ? Hand::Right : Hand::Left,
                 GloveRole::Secondary);
    return hand == Hand::Left ? GlovePair(local, remote)
                              : GlovePair(remote, local);
}

void combineStates(GlovePair& gloves, Hand localHand,
                   const MockSample& local, const HandState& remote) {
    if (localHand == Hand::Left) {
        gloves.setLeftState(local.left);
        gloves.setRightState(remote.state);
    } else {
        gloves.setLeftState(remote.state);
        gloves.setRightState(local.right);
    }
}
}

Primary::Primary(Hand hand)
    : gloves(makePair(hand)),
      localHand(hand) {}

Primary::Primary(Hand hand, const std::string& peer, std::uint16_t port)
    : gloves(makePair(hand)),
      localHand(hand),
      connection(std::in_place, peer, port) {}

Primary::Primary(Hand hand, const std::string& bindAddress, std::uint16_t listenPort,
                 const std::string& peer, std::uint16_t port,
                 NodeTransport transport, std::chrono::milliseconds timeout)
    : gloves(makePair(hand)),
      localHand(hand),
      udpTimeout(timeout) {
    if (transport == NodeTransport::Tcp) {
        upstream.emplace(bindAddress, listenPort);
        connection.emplace(peer, port);
    } else if (transport == NodeTransport::Udp) {
        udpUpstream.emplace(bindAddress, listenPort);
        udpPeer = peer;
        udpPort = port;
    } else {
        throw std::invalid_argument("Invalid node transport");
    }
}

void Primary::runUdp() {
    std::array<bool, UDP_SAMPLE_COUNT> seen{};
    try {
        UdpSender sender(udpPeer, udpPort);
        udpUpstream->run(UDP_SAMPLE_COUNT, udpTimeout, [&](const std::string& datagram) {
            const auto sample = parseUdpSample(datagram);
            const auto remote = parseHandState(sample.payload);
            if (remote.hand == localHand) {
                throw std::invalid_argument("Secondary must represent the opposite physical hand");
            }
            if (seen[sample.index]) {
                throw std::invalid_argument(
                    "Duplicate UDP sample index: " + std::to_string(sample.index));
            }
            const auto& local = mockSamples().at(sample.index);
            combineStates(gloves, localHand, local, remote);
            sender.send(serializeUdpSample(sample.index, serializeState(gloves.getState())));
            seen[sample.index] = true;
        });
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string(error.what()) + missingUdpSamples(seen));
    }
}

void Primary::runLocal() {
    for (const auto& sample : mockSamples()) {
        gloves.setLeftState(sample.left);
        gloves.setRightState(sample.right);
        printMockState(std::cout, gloves.getState());
    }
}

void Primary::runTcp() {
    try {
        connection->connect();
        if (upstream) {
            std::size_t accepted = 0;
            upstream->run([&](const std::string& payload) {
                const auto remote = parseHandState(payload);
                if (remote.hand == localHand) {
                    throw std::invalid_argument(
                        "Secondary must represent the opposite physical hand");
                }
                if (accepted == mockSamples().size()) {
                    throw std::runtime_error("Unexpected Secondary sample after five samples");
                }
                // Deterministic fixture pairing, not synchronized sensor sampling.
                const auto& local = mockSamples()[accepted];
                combineStates(gloves, localHand, local, remote);
                connection->send(serializeState(gloves.getState()) + '\n');
                ++accepted;
            });
            if (accepted != mockSamples().size()) {
                throw std::runtime_error("Secondary disconnected before five samples");
            }
        } else {
            for (const auto& sample : mockSamples()) {
                gloves.setLeftState(sample.left);
                gloves.setRightState(sample.right);
                connection->send(serializeState(gloves.getState()) + '\n');
            }
        }
        connection->disconnect();
    } catch (...) {
        connection->disconnect();
        throw;
    }
}

void Primary::run() {
    if (udpUpstream) {
        runUdp();
    } else if (connection) {
        runTcp();
    } else {
        runLocal();
    }
}
