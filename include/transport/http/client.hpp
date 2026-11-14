#ifndef GLOVER_HTTP_CLIENT_HPP
#define GLOVER_HTTP_CLIENT_HPP

#include <cstdint>
#include <string>

struct HttpResponse {
    int status;
    std::string body;
};

class HttpClient {
    std::string host;
    std::uint16_t port;

public:
    // Plain HTTP. Host is a hostname or IPv4 address, without a scheme or port.
    // Construction validates configuration; each request opens its own connection.
    HttpClient(const std::string& host, std::uint16_t port);

    // Paths start with '/' and may include an already percent-encoded query.
    // Connection/read/write timeouts are 5 seconds; request budget is 15 seconds.
    // DNS resolution may be subject to platform resolver timeout behavior.
    // No retries or redirect following. HTTP error statuses are returned normally.
    // Invalid arguments throw std::invalid_argument; request failures throw
    // std::runtime_error. Bodies are opaque bytes, including embedded NULs.
    HttpResponse get(const std::string& path) const;
    HttpResponse post(const std::string& path, const std::string& body,
                      const std::string& contentType) const;
};

#endif // GLOVER_HTTP_CLIENT_HPP
