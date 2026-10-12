#include <transport/ble/central.hpp>
#include <transport/ble/peripheral.hpp>
#include <charconv>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--help") {
            std::cout << "Isolated macOS BLE experiment (not the Glover CLI)\n"
                      << "Usage: ble-feasibility central|peripheral [--timeout-ms 1..60000]\n"
                      << "Timeout is per stage; default 10000 ms. Physical exchange requires a real peer.\n";
            return 0;
        }
        if (argc != 2 && argc != 4) throw std::invalid_argument("Expected central|peripheral [--timeout-ms N]");
        const std::string role = argv[1];
        if (role != "central" && role != "peripheral") throw std::invalid_argument("Role must be central or peripheral");
        int milliseconds = 10000;
        if (argc == 4) {
            if (std::string(argv[2]) != "--timeout-ms") throw std::invalid_argument("Unknown option");
            const std::string text = argv[3];
            const auto result = std::from_chars(text.data(), text.data() + text.size(), milliseconds);
            if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || milliseconds < 1 || milliseconds > 60000) {
                throw std::invalid_argument("Timeout must be an integer from 1 to 60000 ms");
            }
        }
        if (role == "central") BleCentral{}.run(std::chrono::milliseconds(milliseconds));
        else BlePeripheral{}.run(std::chrono::milliseconds(milliseconds));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "BLE experiment incomplete: " << error.what() << '\n';
        return 1;
    }
}
