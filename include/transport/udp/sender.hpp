#ifndef GLOVER_UDP_SENDER_HPP
#define GLOVER_UDP_SENDER_HPP

#include <cstddef>
#include <cstdint>
#include <string>

class UdpSender {
    std::uint32_t destinationNetworkOrder;
    std::uint16_t portNetworkOrder;
    int socketFd = -1;

public:
    static constexpr std::size_t MAX_MESSAGE_BYTES = 4096;
    // Numeric IPv4; port 1..65535. Construction creates an unconnected socket.
    UdpSender(const std::string& address, int port);
    ~UdpSender();
    UdpSender(const UdpSender&) = delete;
    UdpSender& operator=(const UdpSender&) = delete;
    UdpSender(UdpSender&&) = delete;
    UdpSender& operator=(UdpSender&&) = delete;
    // Exactly one datagram, including empty/binary payloads; no LF added.
    // Success is local acceptance, not confirmation of delivery.
    void send(const std::string& message);
};

#endif
