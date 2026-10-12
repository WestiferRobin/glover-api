#ifndef GLOVER_TCP_FRAMING_HPP
#define GLOVER_TCP_FRAMING_HPP

#include <transport/tcp/server.hpp>
#include <stdexcept>
#include <string_view>

namespace tcp_detail {
class LineFramer {
    std::string pending;

public:
    void consume(std::string_view bytes,
                 const std::function<void(const std::string&)>& onMessage) {
        for (char byte : bytes) {
            if (byte == '\n') {
                onMessage(pending);
                pending.clear();
            } else {
                if (pending.size() == TcpServer::MAX_MESSAGE_BYTES) {
                    throw std::length_error("TCP message exceeds 4096 bytes");
                }
                pending.push_back(byte);
            }
        }
    }

    void finish() const {
        if (!pending.empty()) {
            throw std::runtime_error("TCP EOF with incomplete message (missing LF)");
        }
    }
};
}

#endif
