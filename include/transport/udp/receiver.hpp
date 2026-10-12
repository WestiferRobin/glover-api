#ifndef GLOVER_UDP_RECEIVER_HPP
#define GLOVER_UDP_RECEIVER_HPP
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

class UdpReceiver {
    std::uint32_t localAddress;
    std::uint16_t port;
public:
    static constexpr std::size_t MAX_MESSAGE_BYTES = 4096;
    UdpReceiver(const std::string& address, int port);
    UdpReceiver(const UdpReceiver&) = delete;
    UdpReceiver& operator=(const UdpReceiver&) = delete;
    UdpReceiver(UdpReceiver&&) = delete;
    UdpReceiver& operator=(UdpReceiver&&) = delete;
    // Each run binds a fresh socket. Count and timeout must be positive.
    // Timeout is a total session budget (1..INT_MAX milliseconds), not per packet.
    // Missing packets throw; no EOF exists. Callback references last only for
    // the call. Callbacks are synchronous and cannot be preempted by the timeout.
    void run(std::size_t count, std::chrono::milliseconds timeout,
             const std::function<void(const std::string&)>& onMessage);
};
#endif
