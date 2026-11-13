#ifndef GLOVER_HTTP_SERVER_HPP
#define GLOVER_HTTP_SERVER_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
    std::string contentType;
    std::string target; // Original request target, including the unmodified query.
};

// Separate from the client's response: server handlers also select a media type.
struct HttpServerResponse {
    int status = 200;
    std::string body;
    std::string contentType = "text/plain";
};

class HttpServer {
    struct Impl;
    std::unique_ptr<Impl> impl;

public:
    using Handler = std::function<HttpServerResponse(const HttpRequest&)>;

    // Numeric IPv4 and port 1..65535; construction does not bind.
    // Plain HTTP, 1 MiB body limit, 5-second read/write timeouts.
    // Multipart requests are rejected (415), never silently converted to fields.
    HttpServer(const std::string& address, std::uint16_t port);
    ~HttpServer();
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    HttpServer(HttpServer&&) = delete;
    HttpServer& operator=(HttpServer&&) = delete;

    // Register exact decoded paths before run(); no query strings or patterns.
    // Duplicate method/path pairs and empty handlers are rejected.
    // Callbacks may run concurrently; captured application state is caller-owned.
    void get(const std::string& path, Handler handler);
    void post(const std::string& path, Handler handler);

    // Blocks until stopped and workers finish. Single-use, including failed runs.
    // Startup/listener failures throw std::runtime_error. Handler exceptions
    // become generic 500 responses. isRunning() reports accept-loop readiness.
    void run();
    bool isRunning() const;
    // Thread-safe, repeatable, also cancels a run not yet started. Not signal-safe.
    // Does not interrupt application callbacks. Before destruction, stop and join
    // the caller-owned run thread; destruction must not race any member call.
    void stop();
};

#endif // GLOVER_HTTP_SERVER_HPP
