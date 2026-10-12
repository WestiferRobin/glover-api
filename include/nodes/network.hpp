#ifndef GLOVER_NODE_NETWORK_HPP
#define GLOVER_NODE_NETWORK_HPP

#include <chrono>

// Node configuration only; transport implementations have no shared base class.
enum class NodeTransport {
    Tcp,
    Udp
};
inline constexpr std::chrono::milliseconds UDP_SESSION_TIMEOUT{5000};

#endif
