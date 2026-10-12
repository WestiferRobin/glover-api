#ifndef GLOVER_UDP_ENVELOPE_HPP
#define GLOVER_UDP_ENVELOPE_HPP

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

inline constexpr std::size_t UDP_SAMPLE_COUNT = 5;
struct UdpSample {
    std::size_t index;
    // View into the input datagram; do not retain beyond its lifetime.
    std::string_view payload;
};
// Experimental envelope: "U1 <0..4> <unchanged state payload>", without LF.
std::string serializeUdpSample(std::size_t index, std::string_view payload);
UdpSample parseUdpSample(std::string_view datagram);
std::string missingUdpSamples(const std::array<bool, UDP_SAMPLE_COUNT>& seen);

#endif
