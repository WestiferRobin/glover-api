#include <cli/parser.hpp>
#include <nodes/primary.hpp>
#include <nodes/secondary.hpp>
#include <nodes/client.hpp>

#include <arpa/inet.h>
#include <charconv>
#include <climits>
#include <map>
#include <ostream>
#include <stdexcept>
#include <utility>

namespace cli {

namespace {

std::map<std::string, std::string> parseOptions(
    const std::string& command, int argc, char* argv[]) {
    std::map<std::string, std::string> options;
    for (int i = 2; i < argc; i += 2) {
        const std::string key = argv[i];
        const bool allowed = key == "--transport" || key == "--port" ||
            ((command == "primary" || command == "client") && key == "--timeout-ms") ||
            (command == "client" && key == "--bind") ||
            ((command == "primary" || command == "secondary") &&
             (key == "--peer" || key == "--hand")) ||
            (command == "primary" && (key == "--bind" || key == "--listen-port"));
        if (!allowed) {
            throw std::invalid_argument("Unsupported option: " + key);
        }
        if (i + 1 >= argc || std::string(argv[i + 1]).starts_with("--")) {
            throw std::invalid_argument("Missing value for " + key);
        }
        if (!options.emplace(key, argv[i + 1]).second) {
            throw std::invalid_argument("Duplicate option: " + key);
        }
    }
    return options;
}

} // namespace

void printHelp(std::ostream& output) {
    output
        << "Usage: glove [command] [network options]\n\n"
        << "Commands:\n"
        << "  mock        Run the five-state glove simulation (default)\n"
        << "  primary     Run the primary glove node\n"
        << "  secondary   Run the secondary glove node\n"
        << "  client      Run the laptop client node\n\n"
        << "Options:\n"
        << "  -h, --help  Show this help message\n\n"
        << "TCP modes (numeric IPv4, port 1-65535):\n"
        << "  primary --transport tcp --peer ADDRESS --port PORT [--hand left|right]\n"
        << "    Add --bind ADDRESS --listen-port PORT for three-node mode.\n"
        << "  secondary --transport tcp --hand left|right --peer ADDRESS --port PORT\n"
        << "  client  --transport tcp --bind ADDRESS --port PORT\n"
        << "UDP three-node mode: use --transport udp with the same endpoints and hands.\n"
        << "  UDP primary requires --bind and --listen-port in addition to --peer and --port.\n"
        << "  UDP primary/client accept --timeout-ms N (1..2147483647; default 5000).\n"
        << "  Start Client, then Primary, then Secondary; wait for each receiver to bind.\n"
        << "Primary defaults to right; Secondary requires a hand in network mode.\n"
        << "Bare primary stays local; bare secondary/client remain startup smoke tests.\n";
}

ParsedCommand parse(int argc, char* argv[]) {
    const std::string command = argc == 1 ? "mock" : argv[1];
    if (argc > 2 && command != "primary" && command != "client" && command != "secondary") {
        throw std::invalid_argument("This command does not accept options");
    }
    if (command == "--help" || command == "-h") {
        return {Action::Help, {}, {}};
    }
    if (command == "mock") {
        return {Action::Mock, {}, {}};
    }

    const auto options = parseOptions(command, argc, argv);
    const auto validateAddress = [](const std::string& address) {
        in_addr parsed{};
        if (inet_pton(AF_INET, address.c_str(), &parsed) != 1) {
            throw std::invalid_argument("Expected a numeric IPv4 address");
        }
    };
    const auto parsePort = [](const std::string& text) {
        unsigned int number = 0;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), number);
        if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
            number == 0 || number > 65535) {
            throw std::invalid_argument("Port must be an integer from 1 to 65535");
        }
        return static_cast<std::uint16_t>(number);
    };
    NodeTransport transport = NodeTransport::Tcp;
    auto timeout = UDP_SESSION_TIMEOUT;
    Hand hand = Hand::Right;
    std::uint16_t listenPort = 0;
    std::string address;
    std::uint16_t port = 0;
    if (!options.empty()) {
        const std::string addressKey = command == "client" ? "--bind" : "--peer";
        if (!options.contains("--transport") || !options.contains("--port") ||
            !options.contains(addressKey)) {
            throw std::invalid_argument(
                "Network mode requires --transport, " + addressKey + " and --port");
        }
        const auto& transportName = options.at("--transport");
        if (transportName != "tcp" && transportName != "udp") {
            throw std::invalid_argument("Transport must be tcp or udp");
        }
        transport = transportName == "udp" ? NodeTransport::Udp : NodeTransport::Tcp;
        if (options.contains("--timeout-ms")) {
            if (transport != NodeTransport::Udp) {
                throw std::invalid_argument("--timeout-ms is only supported by UDP receivers");
            }
            const auto& text = options.at("--timeout-ms");
            int number = 0;
            const auto result = std::from_chars(text.data(), text.data() + text.size(), number);
            if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
                number <= 0) {
                throw std::invalid_argument(
                    "Timeout must be an integer from 1 to " + std::to_string(INT_MAX));
            }
            timeout = std::chrono::milliseconds(number);
        }
        address = options.at(addressKey);
        validateAddress(address);
        port = parsePort(options.at("--port"));
        if (command == "secondary" && !options.contains("--hand")) {
            throw std::invalid_argument("Secondary network mode requires --hand");
        }
        if (options.contains("--hand")) {
            const auto& value = options.at("--hand");
            if (value != "left" && value != "right") {
                throw std::invalid_argument("Hand must be left or right");
            }
            hand = value == "left" ? Hand::Left : Hand::Right;
        }
        if (command == "primary") {
            if (options.contains("--bind") != options.contains("--listen-port")) {
                throw std::invalid_argument(
                    "Primary inbound mode requires both --bind and --listen-port");
            }
            if (transport == NodeTransport::Udp && !options.contains("--bind")) {
                throw std::invalid_argument("UDP primary requires --bind and --listen-port");
            }
            if (options.contains("--bind")) {
                validateAddress(options.at("--bind"));
                listenPort = parsePort(options.at("--listen-port"));
            }
        }
    }

    std::unique_ptr<Node> node;

    if (command == "primary") {
        if (options.empty()) {
            node = std::make_unique<Primary>();
        } else if (listenPort != 0) {
            node = std::make_unique<Primary>(
                hand, options.at("--bind"), listenPort, address, port, transport, timeout);
        } else {
            node = std::make_unique<Primary>(hand, address, port);
        }
    } else if (command == "secondary") {
        if (options.empty()) {
            node = std::make_unique<Secondary>();
        } else {
            node = std::make_unique<Secondary>(hand, address, port, transport);
        }
    } else if (command == "client") {
        if (options.empty()) {
            node = std::make_unique<Client>();
        } else {
            node = std::make_unique<Client>(address, port, transport, timeout);
        }
    } else {
        return {Action::UnknownCommand, {}, command};
    }

    return {Action::Node, std::move(node), {}};
}

} // namespace cli
