#include <transport/udp/detail/udp_envelope.hpp>
#include <stdexcept>

std::string serializeUdpSample(std::size_t index, std::string_view payload) {
    if (index >= UDP_SAMPLE_COUNT || payload.empty()) {
        throw std::invalid_argument("UDP envelope requires index 0..4 and a payload");
    }
    return "U1 " + std::to_string(index) + " " + std::string(payload);
}

UdpSample parseUdpSample(std::string_view datagram) {
    if (datagram.size() < 6 || datagram.substr(0, 3) != "U1 " || datagram[4] != ' ') {
        throw std::invalid_argument("Invalid UDP envelope: expected U1 <0..4> <payload>");
    }
    if (datagram[3] < '0' || datagram[3] > '4') {
        throw std::invalid_argument("Invalid UDP sample index: expected 0..4");
    }
    return {static_cast<std::size_t>(datagram[3] - '0'), datagram.substr(5)};
}

std::string missingUdpSamples(const std::array<bool, UDP_SAMPLE_COUNT>& seen) {
    std::string result;
    for (std::size_t i = 0; i < seen.size(); ++i) {
        if (!seen[i]) {
            if (!result.empty()) result += ',';
            result += std::to_string(i);
        }
    }
    return "; missing sample indices: " + (result.empty() ? std::string("none") : result);
}
