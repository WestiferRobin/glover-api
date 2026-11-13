#include <transport/http/server.hpp>
#include <httplib.h>

#include <arpa/inet.h>
#include <algorithm>
#include <map>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace {
bool hasControlCharacters(const std::string& value) {
    return std::any_of(value.begin(), value.end(), [](unsigned char byte) {
        return byte < 32 || byte == 127;
    });
}

std::string exactPattern(const std::string& path) {
    std::string pattern = "^";
    for (char byte : path) {
        // Encode ':' to avoid cpp-httplib's path-parameter matcher.
        if (byte == ':') {
            pattern += "\\x3a";
        } else {
            if (std::string(".^$|()[]{}*+?\\").find(byte) != std::string::npos) {
                pattern += '\\';
            }
            pattern += byte;
        }
    }
    return pattern + '$';
}
} // namespace

struct HttpServer::Impl {
    httplib::Server server;
    std::string address;
    std::uint16_t port;
    std::mutex lifecycle;
    bool started = false;
    bool stopped = false;
    // Method availability only; cpp-httplib owns and dispatches the handlers.
    std::map<std::string, unsigned> routes;

    Impl(const std::string& address, std::uint16_t port)
        : address(address), port(port) {
        in_addr parsed{};
        if (address.find('\0') != std::string::npos ||
            inet_pton(AF_INET, address.c_str(), &parsed) != 1 || port == 0) {
            throw std::invalid_argument("HTTP server requires numeric IPv4 and port 1..65535");
        }
        server.set_address_family(AF_INET);
        server.set_socket_options([](socket_t socket) {
            // Permit restart after close, but not concurrent listeners on one port.
            // cpp-httplib's default enables SO_REUSEPORT on supported platforms.
            const int enabled = 1;
            ::setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));
        });
        server.set_payload_max_length(1024 * 1024);
        server.set_read_timeout(5, 0);
        server.set_write_timeout(5, 0);
        server.set_keep_alive_max_count(1);
        server.set_pre_routing_handler([this](const httplib::Request& request,
                                              httplib::Response& response) {
            const auto route = routes.find(request.path);
            const unsigned method = request.method == "GET" ? 1 :
                                    request.method == "POST" ? 2 : 0;
            if (route == routes.end()) {
                response.status = 404;
                response.set_content("Not Found", "text/plain");
            } else if ((route->second & method) == 0) {
                response.status = 405;
                response.set_header("Allow", route->second == 3 ? "GET, POST" :
                                             route->second == 1 ? "GET" : "POST");
                response.set_content("Method Not Allowed", "text/plain");
            } else if (request.is_multipart_form_data()) {
                response.status = 415;
                response.set_content("Multipart bodies are not supported", "text/plain");
            } else {
                return httplib::Server::HandlerResponse::Unhandled;
            }
            return httplib::Server::HandlerResponse::Handled;
        });
        server.set_exception_handler([](const httplib::Request&,
                                        httplib::Response& response, std::exception_ptr) {
            response.status = 500;
            response.set_content("Internal Server Error", "text/plain");
        });
    }

    void add(const std::string& path, Handler handler, unsigned method) {
        std::lock_guard lock(lifecycle);
        if (started || stopped) {
            throw std::logic_error("HTTP routes must be registered before run or stop");
        }
        if (!handler || path.empty() || path.front() != '/' ||
            path.find_first_of(" ?#") != std::string::npos || hasControlCharacters(path)) {
            throw std::invalid_argument("HTTP route requires a handler and an absolute path without query or fragment");
        }
        const auto existing = routes.find(path);
        if (existing != routes.end() && (existing->second & method)) {
            throw std::invalid_argument("Duplicate HTTP method and path");
        }
        auto callback = [handler = std::move(handler)](const httplib::Request& request,
                                                       httplib::Response& response) {
            const auto result = handler({request.method, request.path, request.body,
                                         request.get_header_value("Content-Type"), request.target});
            if (result.status < 200 || result.status > 599 ||
                result.contentType.empty() || hasControlCharacters(result.contentType)) {
                throw std::invalid_argument("Invalid HTTP handler response");
            }
            response.status = result.status;
            response.set_content(result.body, result.contentType);
        };
        const auto pattern = exactPattern(path);
        if (method == 1) {
            server.Get(pattern, std::move(callback));
        } else {
            server.Post(pattern, std::move(callback));
        }
        routes[path] |= method;
    }
};

HttpServer::HttpServer(const std::string& address, std::uint16_t port)
    : impl(std::make_unique<Impl>(address, port)) {}

HttpServer::~HttpServer() {
    impl->server.stop();
}

void HttpServer::get(const std::string& path, Handler handler) {
    impl->add(path, std::move(handler), 1);
}

void HttpServer::post(const std::string& path, Handler handler) {
    impl->add(path, std::move(handler), 2);
}

void HttpServer::run() {
    {
        std::lock_guard lock(impl->lifecycle);
        if (impl->started || impl->stopped) {
            throw std::logic_error("HTTP server run is single-use and cannot follow stop");
        }
        impl->started = true;
        if (!impl->server.bind_to_port(impl->address, impl->port)) {
            throw std::runtime_error("HTTP bind/listen failed at " + impl->address +
                                     ":" + std::to_string(impl->port));
        }
    }
    bool success;
    try {
        success = impl->server.listen_after_bind();
    } catch (...) {
        impl->server.stop();
        throw;
    }
    std::lock_guard lock(impl->lifecycle);
    impl->server.stop();
    if (!success && !impl->stopped) {
        throw std::runtime_error("HTTP listener failed");
    }
}

bool HttpServer::isRunning() const {
    std::lock_guard lock(impl->lifecycle);
    return !impl->stopped && impl->server.is_running();
}

void HttpServer::stop() {
    std::lock_guard lock(impl->lifecycle);
    impl->stopped = true;
    impl->server.stop();
}
