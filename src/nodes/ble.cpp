#include <nodes/primary.hpp>
#include <nodes/secondary.hpp>
#include <nodes/client.hpp>
#include <transport/ble/central.hpp>
#include <transport/ble/peripheral.hpp>
#include <transport/udp/sender.hpp>
#include <mock.hpp>
#include <transport/detail/state_codec.hpp>
#include <transport/udp/detail/udp_envelope.hpp>
#include <transport/ble/detail/ble_message.hpp>
#include <iostream>
#include <arpa/inet.h>
#include <stdexcept>

namespace {
void available() {
#ifndef GLOVER_ENABLE_BLE
    throw std::runtime_error("BLE is not enabled in this build; run make ble-node on macOS");
#endif
}
std::vector<std::string> receiveBle(const BleConfig& config) {
    available();
#ifdef GLOVER_ENABLE_BLE
    return BleCentral{}.receive(config, 5).messages;
#else
    (void)config;
    throw std::logic_error("BLE unavailable");
#endif
}
void publishBle(const BleConfig& config, const std::vector<std::string>& payloads) {
    available();
#ifdef GLOVER_ENABLE_BLE
    BlePeripheral{}.publish(config, payloads);
#else
    (void)config; (void)payloads;
#endif
}
void validateLink(const NodeLink& link, bool receiving) {
    if (link.transport == NodeTransport::Ble) validateBleConfig(link.ble);
    else if (link.transport != NodeTransport::Tcp && link.transport != NodeTransport::Udp)
        throw std::invalid_argument("Invalid link transport");
    else {
        in_addr parsed{};
        if (!link.port || link.address.find('\0') != std::string::npos || inet_pton(AF_INET,link.address.c_str(),&parsed)!=1)
            throw std::invalid_argument("Network link needs numeric IPv4 address and nonzero port");
        if (receiving && link.transport==NodeTransport::Udp && (link.udpTimeout.count()<1 || link.udpTimeout.count()>60000))
            throw std::invalid_argument("Independent UDP link timeout must be 1..60000 ms");
    }
}
// Validate a complete batch before changing models or publishing any output.
std::array<std::string,5> receiveLink(const NodeLink& link) {
    std::array<std::string,5> result;
    std::array<bool,5> seen{};
    auto accept = [&](std::size_t index, std::string_view payload) {
        if (index >= 5 || seen[index]) throw std::invalid_argument("Duplicate or out-of-range sample index");
        seen[index] = true; result[index] = payload;
    };
    if (link.transport == NodeTransport::Ble) {
        for (const auto& message : receiveBle(link.ble)) {
            const auto sample = parseBleMessage(message); accept(sample.index, sample.payload);
        }
    } else if (link.transport == NodeTransport::Udp) {
        UdpReceiver receiver(link.address,link.port);
        receiver.run(5,link.udpTimeout,[&](const std::string& bytes) {
            const auto sample=parseUdpSample(bytes); accept(sample.index,sample.payload);
        });
    } else {
        std::size_t index=0;
        TcpServer receiver(link.address,link.port);
        receiver.run([&](const std::string& bytes) { accept(index++,bytes); });
    }
    for (bool present : seen) if (!present) throw std::runtime_error("Incomplete five-state batch");
    return result;
}
void sendLink(const NodeLink& link, const std::array<std::string,5>& payloads) {
    if (link.transport == NodeTransport::Ble) {
        std::vector<std::string> messages;
        for (std::size_t i=0;i<5;++i) messages.push_back(bleMessage(i,payloads[i]));
        publishBle(link.ble,messages);
    } else if (link.transport == NodeTransport::Udp) {
        UdpSender sender(link.address,link.port);
        for (std::size_t i=0;i<5;++i) sender.send(serializeUdpSample(i,payloads[i]));
    } else {
        TcpClient sender(link.address,link.port); sender.connect();
        for (const auto& bytes : payloads) sender.send(bytes+'\n');
        sender.disconnect();
    }
}
}

Secondary::Secondary(Hand hand, const BleConfig& config) : glove(hand,GloveRole::Secondary),bleMode(config) {
    validateBleConfig(config);
}
Client::Client(const BleConfig& config) : bleMode(config) { validateBleConfig(config); }
Primary::Primary(Hand hand, const PrimaryLinks& links) : Primary(hand) {
    validateLink(links.outbound, false);
    if (links.inbound) validateLink(*links.inbound, true);
    if (links.outbound.transport != NodeTransport::Ble && (!links.inbound || links.inbound->transport != NodeTransport::Ble))
        throw std::invalid_argument("Independent link mode requires at least one BLE link");
    if (links.inbound && links.inbound->transport == NodeTransport::Ble && links.outbound.transport == NodeTransport::Ble) {
        auto a=links.inbound->ble.service, b=links.outbound.ble.service;
        for (char& c:a) if(c>='A'&&c<='F') c+= 'a'-'A';
        for (char& c:b) if(c>='A'&&c<='F') c+= 'a'-'A';
        if (a==b) throw std::invalid_argument("Two BLE links require distinct service UUIDs to avoid cross-discovery");
    }
    bleMode=links;
}
void Secondary::runBle() {
    available();
    std::vector<std::string> messages;
    for (std::size_t i=0;i<mockSamples().size();++i) {
        const auto& sample=mockSamples()[i];
        glove.setState(glove.getHand()==Hand::Left ? sample.left : sample.right);
        messages.push_back(bleMessage(i,serializeHandState({glove.getHand(),glove.getState()})));
    }
    publishBle(*bleMode,messages);
}
void Client::runBle() {
    available();
    NodeLink link; link.ble=*bleMode;
    const auto payloads=receiveLink(link);
    std::array<std::array<bool,PAIR_FINGER_COUNT>,5> states;
    for (std::size_t i=0;i<5;++i) states[i]=parseState(payloads[i]);
    for (const auto& state:states) std::cout << serializeState(state) << '\n';
}
void Primary::runBle() {
    available();
    std::array<HandState,5> remote{};
    if (bleMode->inbound) {
        const auto payloads=receiveLink(*bleMode->inbound);
        for (std::size_t i=0;i<5;++i) {
            remote[i]=parseHandState(payloads[i]);
            if (remote[i].hand==localHand) throw std::invalid_argument("Secondary must represent opposite physical hand");
        }
    }
    std::array<std::string,5> combined;
    for (std::size_t i=0;i<5;++i) {
        const auto& sample=mockSamples()[i];
        gloves.setLeftState(bleMode->inbound && localHand==Hand::Right ? remote[i].state : sample.left);
        gloves.setRightState(bleMode->inbound && localHand==Hand::Left ? remote[i].state : sample.right);
        combined[i]=serializeState(gloves.getState());
    }
    sendLink(bleMode->outbound,combined);
}
