#ifndef GLOVER_BLE_CENTRAL_HPP
#define GLOVER_BLE_CENTRAL_HPP
#include "config.hpp"

class BleCentral {
public:
    BleCentral() = default;
    BleCentral(const BleCentral&) = delete;
    BleCentral& operator=(const BleCentral&) = delete;
    BleCentral(BleCentral&&) = delete;
    BleCentral& operator=(BleCentral&&) = delete;
    // Main thread, finite session. Owns returned binary bytes (including NULs).
    // Reads first, then receives exactly count notifications, then cancels the
    // connection while still subscribed and waits for local disconnect confirmation.
    // No partial batch is returned on failure. Operational errors throw standard
    // C++ exceptions. Callback C++ exceptions are contained, then rethrown with
    // their original type after cleanup; Objective-C exceptions are not translated.
    BleReceived receive(const BleConfig& config, std::size_t count);
    // Compatibility feasibility fixture, using the same real payload API.
    void run(std::chrono::milliseconds stageTimeout = std::chrono::seconds(10));
};
#endif
