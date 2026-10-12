#ifndef GLOVER_NODE_NETWORK_HPP
#define GLOVER_NODE_NETWORK_HPP
#include <chrono>
#include <transport/ble/config.hpp>
#include <optional>
#include <cstdint>

// Node configuration only; transport implementations have no shared base class.
enum class NodeTransport { Tcp, Udp, Ble };
inline constexpr std::chrono::milliseconds UDP_SESSION_TIMEOUT{5000};
// Independent finite-batch link configuration, not a transport base class.
struct NodeLink {
    NodeTransport transport = NodeTransport::Ble;
    std::string address;
    std::uint16_t port = 0;
    BleConfig ble; // Used only by BLE links; timeout is per stage.
    // UDP receive-session budget (1..60000 ms); ignored for TCP, BLE, and UDP sending.
    // Direct C++ default preserves the former effective BLE-derived 10 seconds.
    std::chrono::milliseconds udpTimeout{10000};
};
struct PrimaryLinks {
    std::optional<NodeLink> inbound; // absent: explicit two-hand local fixture mode
    NodeLink outbound;
};
#endif
