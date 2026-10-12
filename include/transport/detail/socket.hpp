#ifndef GLOVER_TRANSPORT_SOCKET_HPP
#define GLOVER_TRANSPORT_SOCKET_HPP

#include <unistd.h>

namespace transport_detail {
class Socket {
    int fd;

public:
    explicit Socket(int descriptor) noexcept
        : fd(descriptor) {}

    ~Socket() {
        if (fd >= 0) {
            ::close(fd);
        }
    }

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    int get() const noexcept {
        return fd;
    }

    int release() noexcept {
        const int result = fd;
        fd = -1;
        return result;
    }
};
}

#endif
