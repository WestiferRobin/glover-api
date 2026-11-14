#include <transport/tcp/client.hpp>
#include <transport/tcp/server.hpp>
#include <transport/udp/sender.hpp>
#include <transport/udp/receiver.hpp>
#include <transport/http/client.hpp>
#include <transport/http/server.hpp>
#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <iterator>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>

namespace {
std::string input() {
    return {std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>()};
}
// Hex is only the driver's output format, not a replacement transport protocol.
void received(const std::string& bytes) {
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned char byte : bytes) std::cout << hex[byte >> 4] << hex[byte & 15];
    std::cout << '\n' << std::flush;
}
void serveHttp(const std::string& address, std::uint16_t port) {
    HttpServer server(address, port);
    server.get("/echo", [](const HttpRequest&) {
        return HttpServerResponse{200, std::string("hello\0http", 10), "application/octet-stream"};
    });
    server.post("/echo", [](const HttpRequest& request) {
        return HttpServerResponse{201, request.body,
            request.contentType.empty() ? "application/octet-stream" : request.contentType};
    });
    server.post("/request", [](const HttpRequest& request) {
        return HttpServerResponse{200, request.method + "\n" + request.path + "\n" +
            request.target + "\n" + request.contentType + "\n" + request.body};
    });
    server.get("/status", [](const HttpRequest&) { return HttpServerResponse{418, "teapot"}; });
    server.get("/throw", [](const HttpRequest&) -> HttpServerResponse {
        throw std::runtime_error("private handler error");
    });
    server.get("/invalid", [](const HttpRequest&) { return HttpServerResponse{199, "invalid"}; });
    server.get("/literal.+", [](const HttpRequest&) { return HttpServerResponse{200, "literal"}; });
    std::exception_ptr error;
    std::atomic<bool> done{false};
    std::thread worker([&] {
        try { server.run(); } catch (...) { error = std::current_exception(); }
        done = true;
    });
    try {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!server.isRunning()) {
            if (done) {
                if (error) std::rethrow_exception(error);
                throw std::runtime_error("HTTP server ended before readiness");
            }
            if (std::chrono::steady_clock::now() >= deadline)
                throw std::runtime_error("HTTP driver readiness deadline exceeded");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        std::cout << "READY\n" << std::flush;
        // Test-only stdin control avoids adding shutdown routes to production.
        pollfd control{STDIN_FILENO, POLLIN, 0};
        if (::poll(&control, 1, 20000) <= 0)
            throw std::runtime_error("HTTP driver control deadline exceeded");
        std::string command;
        if (!std::getline(std::cin, command) || command != "stop")
            throw std::runtime_error("HTTP driver expected stdin stop command");
        server.stop();
        server.stop(); // Existing repeatable lifecycle contract.
        worker.join();
        if (error) std::rethrow_exception(error);
        if (server.isRunning()) throw std::runtime_error("HTTP server still running after stop");
        std::cout << "STOPPED\n";
    } catch (...) {
        server.stop();
        if (worker.joinable()) worker.join();
        throw;
    }
}
}

int main(int argc, char* argv[]) {
    try {
        if (argc < 4) throw std::invalid_argument("driver MODE ADDRESS PORT [mode arguments]");
        const std::string mode = argv[1], address = argv[2];
        const int port = std::stoi(argv[3]);
        if (mode == "udp-receive") {
            if (argc != 6) throw std::invalid_argument("udp-receive requires count and timeout-ms");
            UdpReceiver receiver(address, port);
            receiver.run(std::stoul(argv[4]), std::chrono::milliseconds(std::stoll(argv[5])), received);
        } else if (mode == "udp-send") {
            UdpSender sender(address, port);
            sender.send(input());
        } else {
            if (port < 1 || port > 65535) throw std::invalid_argument("driver port out of range");
            const auto endpoint = static_cast<std::uint16_t>(port);
            if (mode == "tcp-receive") {
                TcpServer server(address, endpoint);
                server.run(received);
            } else if (mode == "tcp-send") {
                TcpClient client(address, endpoint);
                client.connect();
                client.send(input());
                client.disconnect();
            } else if (mode == "http-serve") {
                serveHttp(address, endpoint);
            } else if (mode == "http-get" || mode == "http-post") {
                if (argc < 5) throw std::invalid_argument("HTTP request requires path");
                HttpClient client(address, endpoint);
                const auto response = mode == "http-get" ? client.get(argv[4]) :
                    client.post(argv[4], input(), argc == 6 ? argv[5] : "application/octet-stream");
                std::cout << response.status << '\n';
                std::cout.write(response.body.data(), static_cast<std::streamsize>(response.body.size()));
            } else {
                throw std::invalid_argument("Unknown driver mode: " + mode);
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Driver error: " << error.what() << '\n';
        return 1;
    }
}
