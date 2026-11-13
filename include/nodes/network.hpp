#ifndef GLOVER_NODE_NETWORK_HPP
#define GLOVER_NODE_NETWORK_HPP

#include <chrono>

enum class NodeTransport {
    Tcp,
    Udp,
    Http
};

// Default total receive timeout for a UDP node session.
inline constexpr std::chrono::milliseconds UDP_SESSION_TIMEOUT{5000};

enum class HttpSubmissionMode { Combined, Individual };
inline constexpr std::chrono::milliseconds HTTP_SESSION_TIMEOUT{30000};

#endif // GLOVER_NODE_NETWORK_HPP
