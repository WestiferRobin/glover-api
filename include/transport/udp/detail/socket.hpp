#ifndef GLOVER_UDP_SOCKET_HPP
#define GLOVER_UDP_SOCKET_HPP

#include <arpa/inet.h>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace udp_detail {
inline sockaddr_in endpoint(const std::string& address, int port) {
    if (port < 1 || port > 65535) {
        throw std::invalid_argument("UDP port must be from 1 to 65535");
    }
    sockaddr_in result{};
    result.sin_family = AF_INET;
    result.sin_port = htons(static_cast<std::uint16_t>(port));
    if (address.find('\0') != std::string::npos ||
        ::inet_pton(AF_INET, address.c_str(), &result.sin_addr) != 1) {
        throw std::invalid_argument("UDP requires a numeric IPv4 address");
    }
    return result;
}
}

#endif
