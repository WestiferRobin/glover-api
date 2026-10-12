#include <cli/parser.hpp>
#include <nodes/primary.hpp>
#include <nodes/secondary.hpp>
#include <nodes/client.hpp>

#include <arpa/inet.h>
#include <charconv>
#include <climits>
#include <map>
#include <ostream>
#include <set>
#include <stdexcept>
#include <utility>

// Preserve the existing independent-link validation order and diagnostics.
namespace {
int number(const std::string& value,int max) {
    int n=0;const auto r=std::from_chars(value.data(),value.data()+value.size(),n);
    if(r.ec!=std::errc{}||r.ptr!=value.data()+value.size()||n<1||n>max)
        throw std::invalid_argument("Invalid positive number or out-of-range value: "+value);
    return n;
}

std::unique_ptr<Node> parseIndependentLinks(const std::string& command,int argc,char** argv) {
    bool selected=false;
    for(int i=2;i<argc;++i) {
        const std::string key=argv[i];
        if(key=="--in-transport"||key=="--out-transport"||(key=="--transport"&&i+1<argc&&std::string(argv[i+1])=="ble")) selected=true;
    }
    if(!selected) return {};
    if(command!="primary"&&command!="secondary"&&command!="client") throw std::invalid_argument("BLE requires a node command");
    std::map<std::string,std::string> options;
    for(int i=2;i<argc;i+=2) {
        const std::string key=argv[i];
        if(i+1==argc||std::string(argv[i+1]).starts_with("--")) throw std::invalid_argument("Missing value for "+key);
        if(!options.emplace(key,argv[i+1]).second) throw std::invalid_argument("Duplicate option: "+key);
    }
    std::set<std::string> used;
    const auto get=[&](const std::string& key,const std::string& fallback) {
        auto it=options.find(key);if(it==options.end()) return fallback;
        used.insert(key);return it->second;
    };
    const auto required=[&](const std::string& key) {
        auto value=get(key,"");if(value.empty()) throw std::invalid_argument("Missing required option: "+key);return value;
    };
    Hand hand=Hand::Right;
    if(command!="client") {
        auto value=command=="secondary"?required("--hand"):get("--hand","right");
        if(value!="left"&&value!="right") throw std::invalid_argument("Hand must be left or right");
        hand=value=="left"?Hand::Left:Hand::Right;
    }
    auto ble=[&](const std::string& prefix,const std::string& role) {
        BleConfig c;
        if(get(prefix=="--" ? "--ble-role" : prefix+"role",role)!=role) throw std::invalid_argument("This link requires BLE role "+role);
        c.service=get(prefix+"service",c.service);
        c.characteristic=get(prefix+"characteristic",c.characteristic);
        c.timeout=std::chrono::milliseconds(number(get(prefix+"timeout-ms","10000"),60000));
        validateBleConfig(c);return c;
    };
    std::unique_ptr<Node> result;
    if(command=="primary") {
        PrimaryLinks links;
        const auto global=get("--transport","");
        if(!global.empty()&&global!="ble") throw std::invalid_argument("Use --in-transport/--out-transport for independent links");
        if(!global.empty()&&options.contains("--out-transport")) throw std::invalid_argument("Do not combine --transport and --out-transport");
        auto link=[&](const std::string& which,const std::string& transport) {
            NodeLink l;
            const std::string prefix="--"+which+"-";
            if(transport=="ble") {l.transport=NodeTransport::Ble;l.ble=ble(prefix,which=="in"?"central":"peripheral");}
            else if(transport=="tcp"||transport=="udp") {
                l.transport=transport=="tcp"?NodeTransport::Tcp:NodeTransport::Udp;
                l.address=required(which=="in"?"--bind":"--peer");
                in_addr parsed{};if(inet_pton(AF_INET,l.address.c_str(),&parsed)!=1) throw std::invalid_argument("Expected numeric IPv4 address");
                l.port=static_cast<std::uint16_t>(number(required(which=="in"?"--listen-port":"--port"),65535));
                if(transport=="udp"&&which=="in") l.udpTimeout=std::chrono::milliseconds(number(get(prefix+"timeout-ms","5000"),60000));
            } else throw std::invalid_argument("Transport must be tcp, udp, or ble");
            return l;
        };
        const auto out=global.empty()?required("--out-transport"):global;
        links.outbound=link("out",out);
        const auto in=get("--in-transport","");
        if(!in.empty()) links.inbound=link("in",in);
        result=std::make_unique<Primary>(hand,links);
    } else {
        if(required("--transport")!="ble") throw std::invalid_argument("Expected --transport ble");
        // Single-link nodes use --ble-role, --service, --characteristic, --timeout-ms.
        const auto c=ble("--",command=="client"?"central":"peripheral");
        if(command=="client") result=std::make_unique<Client>(c);
        else result=std::make_unique<Secondary>(hand,c);
    }
    for(const auto& [key,value]:options) {
        (void)value;
        if(!used.contains(key)) throw std::invalid_argument("Unsupported option for selected links: "+key);
    }
    return result;
}

} // namespace

namespace cli {

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
        << "Bare primary stays local; bare secondary/client remain startup smoke tests.\n"
        << "BLE (optional make ble-node build; real second endpoint required):\n"
        << "  secondary --transport ble --hand left|right [--ble-role peripheral]\n"
        << "  client --transport ble [--ble-role central]\n"
        << "    Single-link BLE options: --service UUID --characteristic UUID --timeout-ms 1..60000\n"
        << "  primary --transport ble [--hand left|right] (local fixtures to BLE)\n"
        << "  primary --in-transport tcp|udp|ble --out-transport tcp|udp|ble [--hand left|right]\n"
        << "    At least one independent link must be BLE; Primary receives a batch then forwards.\n"
        << "    Network endpoints: --bind/--listen-port inbound; --peer/--port outbound.\n"
        << "    BLE link options: --in-role central / --out-role peripheral,\n"
        << "      --in-service/--out-service, --in-characteristic/--out-characteristic,\n"
        << "      --in-timeout-ms/--out-timeout-ms (1..60000, default 10000).\n"
        << "    Two BLE links require distinct service UUIDs. No same-Mac loopback assumed.\n";
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

    if (auto node = parseIndependentLinks(command, argc, argv)) {
        return {Action::Node, std::move(node), {}};
    }
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
            throw std::invalid_argument("Network mode requires --transport, " + addressKey + " and --port");
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
            if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || number <= 0) {
                throw std::invalid_argument("Timeout must be an integer from 1 to " + std::to_string(INT_MAX));
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
                throw std::invalid_argument("Primary inbound mode requires both --bind and --listen-port");
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
        if (options.empty()) node = std::make_unique<Primary>();
        else if (listenPort != 0) {
            node = std::make_unique<Primary>(hand, options.at("--bind"), listenPort, address, port, transport, timeout);
        } else node = std::make_unique<Primary>(hand, address, port);
    } else if (command == "secondary") {
        if (options.empty()) node = std::make_unique<Secondary>();
        else node = std::make_unique<Secondary>(hand, address, port, transport);
    } else if (command == "client") {
        if (options.empty()) node = std::make_unique<Client>();
        else node = std::make_unique<Client>(address, port, transport, timeout);
    } else {
        return {Action::UnknownCommand, {}, command};
    }

    return {Action::Node, std::move(node), {}};
}

} // namespace cli
