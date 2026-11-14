#include <nodes/primary.hpp>
#include <mock.hpp>
#include <codec/glove_json.hpp>
#include <transport/http/client.hpp>
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

Primary::Primary(Hand hand, const std::string& peer, std::uint16_t port,
                 NodeTransport transport, HttpSubmissionMode submission)
    : gloves(makePair(hand)), localHand(hand), httpSubmission(submission) {
    if ((submission != HttpSubmissionMode::Combined && submission != HttpSubmissionMode::Individual) ||
        (submission == HttpSubmissionMode::Individual && transport != NodeTransport::Http))
        throw std::invalid_argument("Individual submission requires HTTP");
    if (transport == NodeTransport::Tcp) {
        connection.emplace(peer, port);
    } else if (transport == NodeTransport::Http) {
        if (!port) throw std::invalid_argument("HTTP port must be nonzero");
        httpPeer = peer;
        httpPort = port;
    } else {
        throw std::invalid_argument("Two-node Primary requires TCP or HTTP");
    }
}

Primary::Primary(Hand hand, const std::string& bindAddress, std::uint16_t listenPort,
                 const std::string& peer, std::uint16_t port,
                 NodeTransport transport, std::chrono::milliseconds timeout,
                 std::optional<NodeTransport> inbound)
    : gloves(makePair(hand)),
      localHand(hand),
      udpTimeout(timeout) {
    if (inbound && transport != NodeTransport::Http)
        throw std::invalid_argument("Explicit upstream transport requires HTTP outbound");
    if (transport == NodeTransport::Http) {
        if (!inbound || (*inbound != NodeTransport::Tcp && *inbound != NodeTransport::Udp) || !port)
            throw std::invalid_argument("HTTP relay requires TCP or UDP upstream and a valid port");
        httpPeer = peer;
        httpPort = port;
        if (*inbound == NodeTransport::Tcp) upstream.emplace(bindAddress, listenPort);
        else udpUpstream.emplace(bindAddress, listenPort);
    } else if (transport == NodeTransport::Tcp) {
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
    std::optional<std::size_t> receivingIndex;
    try {
        std::optional<UdpSender> sender;
        if (!httpPort) sender.emplace(udpPeer, udpPort);
        udpUpstream->run(UDP_SAMPLE_COUNT, udpTimeout, [&](const std::string& datagram) {
            receivingIndex.reset();
            const auto sample = parseUdpSample(datagram);
            receivingIndex = sample.index;
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
            if (httpPort) sendHttp(sample.index);
            else sender->send(serializeUdpSample(sample.index, serializeState(gloves.getState())));
            seen[sample.index] = true;
            receivingIndex.reset();
        });
    } catch (const std::exception& error) {
        throw std::runtime_error((httpPort ? "UDP reception/relay sample " +
            (receivingIndex ? std::to_string(*receivingIndex) : std::string("unknown")) + ": " : "") + std::string(error.what()) + missingUdpSamples(seen));
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
    std::size_t accepted = 0;
    try {
        if (connection) connection->connect();
        if (upstream) {
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
                if (httpPort) sendHttp(accepted);
                else connection->send(serializeState(gloves.getState()) + '\n');
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
        if (connection) connection->disconnect();
    } catch (const std::exception& error) {
        if (connection) connection->disconnect();
        if (httpPort) throw std::runtime_error("TCP reception/relay sample " + std::to_string(accepted) + ": " + error.what());
        throw;
    }
}

void Primary::sendHttp(std::size_t index) {
    try {
        HttpClient client(httpPeer, httpPort);
        const auto body = httpSubmission == HttpSubmissionMode::Combined
            ? glove_json::combinedSubmission(localHand, index, gloves.getState())
            : glove_json::individual(GloveRole::Primary, localHand, index,
                localHand == Hand::Left ? gloves.getLeftState() : gloves.getRightState());
        const auto response = client.post("/gloves", body, "application/json");
        if (response.status != 204)
            throw std::runtime_error("expected HTTP 204, received HTTP " + std::to_string(response.status));
    } catch (const std::exception& error) {
        throw std::runtime_error("HTTP forwarding sample " + std::to_string(index) + " to " +
            httpPeer + ':' + std::to_string(httpPort) + ": " + error.what());
    }
}

void Primary::runHttp() {
    const auto& samples = mockSamples();
    for (std::size_t index = 0; index < samples.size(); ++index) {
        if (httpSubmission == HttpSubmissionMode::Combined || localHand == Hand::Left)
            gloves.setLeftState(samples[index].left);
        if (httpSubmission == HttpSubmissionMode::Combined || localHand == Hand::Right)
            gloves.setRightState(samples[index].right);
        sendHttp(index);
    }
}

void Primary::run() {
    if (udpUpstream) {
        runUdp();
    } else if (upstream || connection) {
        runTcp();
    } else if (httpPort) {
        runHttp();
    } else {
        runLocal();
    }
}
