#ifndef GLOVER_BLE_CONFIG_HPP
#define GLOVER_BLE_CONFIG_HPP
#include "experiment.hpp"
#include <chrono>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>

struct BleConfig {
    std::string service = ble_experiment::SERVICE_UUID;
    std::string characteristic = ble_experiment::CHARACTERISTIC_UUID;
    std::chrono::milliseconds timeout{10000}; // Per stage, not per packet.
};
inline constexpr std::size_t BLE_MAX_PAYLOAD = 512;
inline constexpr std::size_t BLE_MAX_MESSAGES = 64;
inline bool validBleUuid(std::string_view value) {
    if (value.size() != 36) return false;
    for (std::size_t i = 0; i < value.size(); ++i) {
        const char c = value[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) { if (c != '-') return false; }
        else if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}
inline void validateBleConfig(const BleConfig& config) {
    if (!validBleUuid(config.service) || !validBleUuid(config.characteristic))
        throw std::invalid_argument("BLE requires canonical 128-bit service and characteristic UUIDs");
    if (config.timeout.count() < 1 || config.timeout.count() > 60000)
        throw std::invalid_argument("BLE timeout must be 1..60000 ms per stage");
}
inline void validateBleCount(std::size_t count) {
    if (!count || count > BLE_MAX_MESSAGES) throw std::invalid_argument("BLE batch must contain 1..64 messages");
}
inline void validateBlePayload(std::string_view bytes, std::size_t peerLimit = BLE_MAX_PAYLOAD) {
    if (bytes.size() > BLE_MAX_PAYLOAD || bytes.size() > peerLimit)
        throw std::length_error("BLE payload exceeds configured or negotiated notification limit; fragmentation is unsupported");
}
struct BleReceived {
    std::string readValue; // Initial characteristic read, separate from notification messages.
    std::vector<std::string> messages;
};
#endif
