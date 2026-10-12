#ifndef GLOVER_BLE_MESSAGE_HPP
#define GLOVER_BLE_MESSAGE_HPP
#include <string>
#include <string_view>
#include <stdexcept>
#include <transport/ble/config.hpp>
// One complete notification = B1 <index 0..4> <unchanged logical state payload>.
inline std::string bleMessage(std::size_t index, std::string_view payload) {
    if (index > 4 || payload.empty()) throw std::invalid_argument("Invalid BLE sample");
    auto result = "B1 " + std::to_string(index) + " " + std::string(payload);
    validateBlePayload(result);
    return result;
}
struct BleSample { std::size_t index; std::string_view payload; };
inline BleSample parseBleMessage(std::string_view value) {
    validateBlePayload(value);
    if (value.size() < 6 || value.substr(0,3) != "B1 " || value[3] < '0' || value[3] > '4' || value[4] != ' ')
        throw std::invalid_argument("Expected BLE B1 <0..4> <state> message");
    return {static_cast<std::size_t>(value[3]-'0'), value.substr(5)};
}
#endif
