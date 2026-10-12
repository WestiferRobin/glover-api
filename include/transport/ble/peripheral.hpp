#ifndef GLOVER_BLE_PERIPHERAL_HPP
#define GLOVER_BLE_PERIPHERAL_HPP
#include "config.hpp"

class BlePeripheral {
public:
    BlePeripheral() = default;
    BlePeripheral(const BlePeripheral&) = delete;
    BlePeripheral& operator=(const BlePeripheral&) = delete;
    BlePeripheral(BlePeripheral&&) = delete;
    BlePeripheral& operator=(BlePeripheral&&) = delete;
    // Main thread, finite immutable batch. Read returns messages.front(); subscribe
    // sends one whole message per notification. A queue success is not delivery ACK.
    // Requires a read before subscription, matching BleCentral::receive.
    // Waits for unsubscribe/disconnect, then performs bounded local teardown.
    void publish(const BleConfig& config, const std::vector<std::string>& messages);
    // Compatibility feasibility fixture, using the same real payload API.
    void run(std::chrono::milliseconds stageTimeout = std::chrono::seconds(10));
};
#endif
