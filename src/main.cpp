#include <cli/parser.hpp>
#include <mock.hpp>
#include <transport/http/client.hpp>
#include <iostream>

namespace {
void runHttpRequest(const cli::ParsedCommand& command) {
    const auto& options = command.http;
    HttpClient client(options.address, options.port);
    const auto response = command.action == cli::Action::HttpGet
        ? client.get(options.path)
        : client.post(options.path, options.body, options.contentType);
    std::cerr << "HTTP " << response.status << '\n';
    std::cout << response.body;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        auto command = cli::parse(argc, argv);
        switch (command.action) {
        case cli::Action::Help:
            cli::printHelp(std::cout);
            return 0;
        case cli::Action::Mock:
            runMock();
            return 0;
        case cli::Action::Node:
            command.node->run();
            return 0;
        case cli::Action::HttpGet:
        case cli::Action::HttpPost:
            runHttpRequest(command);
            return 0;
        case cli::Action::UnknownCommand:
            std::cerr << "Unknown command: " << command.command << '\n';
            cli::printHelp(std::cerr);
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
