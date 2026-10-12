#include <cli/parser.hpp>
#include <mock.hpp>

#include <exception>
#include <iostream>

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
