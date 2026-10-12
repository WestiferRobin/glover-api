#include <transport/detail/state_codec.hpp>
#include <stdexcept>

std::string serializeState(const std::array<bool, PAIR_FINGER_COUNT>& state) {
    std::string result = "[";
    for (std::size_t i = 0; i < state.size(); ++i) {
        if (i != 0) result += ',';
        result += state[i] ? '1' : '0';
    }
    result += ']';
    return result;
}

std::array<bool, PAIR_FINGER_COUNT> parseState(std::string_view payload) {
    if (payload.size() != 2 * PAIR_FINGER_COUNT + 1 ||
        payload.front() != '[' || payload.back() != ']') {
        throw std::invalid_argument("Invalid state: expected ten bracketed binary values");
    }
    std::array<bool, PAIR_FINGER_COUNT> state{};
    for (std::size_t i = 0; i < state.size(); ++i) {
        const char value = payload[1 + 2 * i];
        if ((value != '0' && value != '1') ||
            (i + 1 < state.size() && payload[2 + 2 * i] != ',')) {
            throw std::invalid_argument("Invalid state: expected binary values separated by commas");
        }
        state[i] = value == '1';
    }
    return state;
}

std::string serializeHandState(const HandState& sample) {
    if (sample.hand != Hand::Left && sample.hand != Hand::Right) {
        throw std::invalid_argument("Invalid physical hand");
    }
    std::string result = sample.hand == Hand::Left ? "S5 L " : "S5 R ";
    for (bool bit : sample.state) result += bit ? '1' : '0';
    return result;
}

HandState parseHandState(std::string_view payload) {
    if (payload.size() != 5 + FINGER_COUNT || payload.substr(0, 3) != "S5 " ||
        (payload[3] != 'L' && payload[3] != 'R') || payload[4] != ' ') {
        throw std::invalid_argument("Invalid five-state payload: expected S5 L/R and five binary digits");
    }
    HandState sample{payload[3] == 'L' ? Hand::Left : Hand::Right, {}};
    for (std::size_t i = 0; i < sample.state.size(); ++i) {
        const char bit = payload[5 + i];
        if (bit != '0' && bit != '1') {
            throw std::invalid_argument("Invalid five-state payload: expected binary digits");
        }
        sample.state[i] = bit == '1';
    }
    return sample;
}
