#include <transport/http/client.hpp>
#include <httplib.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace {
bool hasControlCharacters(const std::string& value) {
    return std::any_of(value.begin(), value.end(), [](unsigned char byte) {
        return byte < 32 || byte == 127;
    });
}

void validatePath(const std::string& path) {
    if (path.empty() || path.front() != '/' || path.starts_with("//") ||
        path.find_first_of(" #") != std::string::npos || hasControlCharacters(path)) {
        throw std::invalid_argument("HTTP path must start with '/' and contain no spaces, fragments or control characters");
    }
}

void configure(httplib::Client& client) {
    client.set_connection_timeout(5, 0);
    client.set_read_timeout(5, 0);
    client.set_write_timeout(5, 0);
    client.set_max_timeout(15000);
    client.set_keep_alive(false);
    client.set_follow_location(false);
    client.set_path_encode(false);
    client.set_decompress(false);
}

HttpResponse response(httplib::Result result, const char* method) {
    if (!result) {
        throw std::runtime_error(std::string("HTTP ") + method + " failed: " +
                                 httplib::to_string(result.error()));
    }
    return {result->status, std::move(result->body)};
}
} // namespace

HttpClient::HttpClient(const std::string& host, std::uint16_t port)
    : host(host), port(port) {
    if (host.empty() || host.find_first_of(" :/?#@\\") != std::string::npos ||
        hasControlCharacters(host)) {
        throw std::invalid_argument("HTTP host must be a hostname or IPv4 address without a scheme or port");
    }
    if (port == 0) {
        throw std::invalid_argument("HTTP port must be from 1 to 65535");
    }
}

HttpResponse HttpClient::get(const std::string& path) const {
    validatePath(path);
    httplib::Client client(host, port);
    configure(client);
    return response(client.Get(path), "GET");
}

HttpResponse HttpClient::post(const std::string& path, const std::string& body,
                              const std::string& contentType) const {
    validatePath(path);
    if (contentType.empty() || hasControlCharacters(contentType)) {
        throw std::invalid_argument("HTTP content type must be nonempty and contain no control characters");
    }
    httplib::Client client(host, port);
    configure(client);
    return response(client.Post(path, body, contentType), "POST");
}
