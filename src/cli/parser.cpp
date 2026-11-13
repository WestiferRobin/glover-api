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

ParsedCommand parseHttp(int argc, char* argv[]) {
    if (argc < 3) {
        throw std::invalid_argument("HTTP requires get or post");
    }
    const std::string subcommand = argv[2];
    const bool post = subcommand == "post";
    if (!post && subcommand != "get") {
        throw std::invalid_argument("Unknown HTTP action: " + subcommand);
    }
    std::map<std::string, std::string> options;
    for (int i = 3; i < argc; i += 2) {
        const std::string key = argv[i];
        const bool allowed = key == "--port" ||
            (key == "--host" || key == "--path") ||
            (post && (key == "--body" || key == "--content-type"));
        if (!allowed) {
            throw std::invalid_argument("Unsupported HTTP option: " + key);
        }
        if (i + 1 >= argc ||
            (key != "--body" && std::string(argv[i + 1]).starts_with("--"))) {
            throw std::invalid_argument("Missing value for " + key);
        }
        if (!options.emplace(key, argv[i + 1]).second) {
            throw std::invalid_argument("Duplicate option: " + key);
        }
    }
    const auto required = [&](const std::string& key) -> const std::string& {
        const auto found = options.find(key);
        if (found == options.end()) {
            throw std::invalid_argument("Missing required HTTP option: " + key);
        }
        if (key != "--body" && found->second.empty()) {
            throw std::invalid_argument("Empty value for " + key);
        }
        return found->second;
    };
    HttpOptions config;
    config.address = required("--host");
    const auto& portText = required("--port");
    unsigned int port = 0;
    const auto parsed = std::from_chars(portText.data(), portText.data() + portText.size(), port);
    if (parsed.ec != std::errc{} || parsed.ptr != portText.data() + portText.size() ||
        port == 0 || port > 65535) {
        throw std::invalid_argument("Port must be an integer from 1 to 65535");
    }
    config.port = static_cast<std::uint16_t>(port);
    config.path = required("--path");
    if (post) {
        config.body = required("--body");
        if (options.contains("--content-type")) {
            config.contentType = required("--content-type");
        }
    }
    return {post ? Action::HttpPost : Action::HttpGet,
            {}, {}, std::move(config)};
}

std::map<std::string, std::string> parseOptions(
    const std::string& command, int argc, char* argv[]) {
    std::map<std::string, std::string> options;
    for (int i = 2; i < argc; i += 2) {
        const std::string key = argv[i];
        const bool allowed = key == "--transport" || key == "--port" ||
            ((command == "primary" || command == "client") && key == "--timeout-ms") ||
            (command == "client" && (key == "--bind" || key == "--stay-open")) ||
            ((command == "client" || command == "primary") && key == "--submission") ||
            ((command == "primary" || command == "secondary") &&
             (key == "--peer" || key == "--hand")) ||
            (command == "primary" && (key == "--bind" || key == "--listen-port" || key == "--upstream-transport"));
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
        << "  http        Run generic HTTP get or post\n\n"
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
        << "Bare primary stays local; bare secondary/client remain startup smoke tests.\n\n"
        << "HTTP API (Client server, combined/relay Primary, and direct glove senders):\n"
        << "  client --transport http --bind ADDRESS --port PORT\n"
        << "    [--submission combined|individual] [--timeout-ms N] [--stay-open true|false]\n"
        << "    Defaults: combined, 30000 ms, true. Routes: GET /health, GET/POST /gloves.\n"
        << "  primary --transport http --peer ADDRESS --port PORT [--hand left|right] [--submission combined|individual]\n"
        << "    Relay: add --upstream-transport tcp|udp --bind ADDRESS --listen-port PORT.\n"
        << "    UDP upstream accepts --timeout-ms N; TCP upstream remains blocking.\n"
        << "  secondary --transport http --hand left|right --peer ADDRESS --port PORT\n"
        << "    Direct senders require Client --submission individual; relays require combined.\n"
        << "  http get --host HOST --port PORT --path PATH\n"
        << "  http post --host HOST --port PORT --path PATH --body BODY [--content-type TYPE]\n"
        << "  POST defaults to text/plain; --body accepts an empty string or option-like text.\n"
        << "  Response body goes unchanged to stdout; HTTP status goes to stderr.\n"
        << "  Complete HTTP responses exit 0, including 4xx/5xx; argument/transport errors exit 1.\n"
        << "  Stop the Client server with Ctrl-C or SIGTERM.\n";
}

ParsedCommand parse(int argc, char* argv[]) {
    const std::string command = argc == 1 ? "mock" : argv[1];
    if (command == "http") {
        return parseHttp(argc, argv);
    }
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
    HttpSubmissionMode submission = HttpSubmissionMode::Combined;
    bool stayOpen = true;
    std::optional<NodeTransport> inbound;
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
        if (transportName != "tcp" && transportName != "udp" && transportName != "http") {
            throw std::invalid_argument("Transport must be tcp, udp or http");
        }
        transport = transportName == "http" ? NodeTransport::Http :
                    transportName == "udp" ? NodeTransport::Udp : NodeTransport::Tcp;
        if (transport == NodeTransport::Http) {

            timeout = HTTP_SESSION_TIMEOUT;
        }
        if (options.contains("--upstream-transport")) {
            if (transport != NodeTransport::Http || !options.contains("--bind") || !options.contains("--listen-port"))
                throw std::invalid_argument("--upstream-transport requires HTTP Primary relay endpoints");
            const auto& value = options.at("--upstream-transport");
            if (value != "tcp" && value != "udp")
                throw std::invalid_argument("Upstream transport must be tcp or udp");
            inbound = value == "tcp" ? NodeTransport::Tcp : NodeTransport::Udp;
        }
        if (command == "primary" && transport == NodeTransport::Http &&
            (options.contains("--bind") || options.contains("--listen-port")) && !inbound)
            throw std::invalid_argument("HTTP relay requires explicit --upstream-transport");
        if (options.contains("--submission") || options.contains("--stay-open")) {
            if (transport != NodeTransport::Http)
                throw std::invalid_argument("--submission and --stay-open require HTTP");
        }
        if (options.contains("--submission")) {
            const auto& value = options.at("--submission");
            if (value != "combined" && value != "individual")
                throw std::invalid_argument("Submission must be combined or individual");
            submission = value == "combined" ? HttpSubmissionMode::Combined : HttpSubmissionMode::Individual;
        }
        if (command == "primary" && inbound && submission != HttpSubmissionMode::Combined)
            throw std::invalid_argument("HTTP relay requires combined submission");
        if (options.contains("--stay-open")) {
            const auto& value = options.at("--stay-open");
            if (value != "true" && value != "false")
                throw std::invalid_argument("Stay-open must be true or false");
            stayOpen = value == "true";
        }
        if (options.contains("--timeout-ms")) {
            if (transport != NodeTransport::Udp && inbound != NodeTransport::Udp && !(transport == NodeTransport::Http && command == "client")) {
                throw std::invalid_argument("--timeout-ms is only supported by UDP receivers or HTTP Client");
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
                hand, options.at("--bind"), listenPort, address, port, transport, timeout, inbound);
        } else {
            node = std::make_unique<Primary>(hand, address, port, transport, submission);
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
        } else if (transport == NodeTransport::Http) {
            node = std::make_unique<Client>(address, port, submission, timeout, stayOpen);
        } else {
            node = std::make_unique<Client>(address, port, transport, timeout);
        }
    } else {
        return {Action::UnknownCommand, {}, command};
    }

    return {Action::Node, std::move(node), {}};
}

} // namespace cli
