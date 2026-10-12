#ifndef GLOVER_CLI_PARSER_HPP
#define GLOVER_CLI_PARSER_HPP

#include <nodes/node.hpp>
#include <iosfwd>
#include <memory>
#include <string>

// Internal application interface; parsing constructs nodes but never runs them.
namespace cli {
enum class Action { Help, Mock, Node, UnknownCommand };

struct ParsedCommand {
    Action action;
    std::unique_ptr<::Node> node; // Present only for Action::Node.
    std::string command;        // Original token for UnknownCommand diagnostics.
};

// Invalid arguments throw; unknown commands retain their separate help/error path.
ParsedCommand parse(int argc, char* argv[]);
void printHelp(std::ostream& output);
} // namespace cli

#endif // GLOVER_CLI_PARSER_HPP
