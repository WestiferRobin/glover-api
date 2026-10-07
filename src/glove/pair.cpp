#include <glove/pair.hpp>

GlovePair::GlovePair(Glove leftGlove, Glove rightGlove)
    : leftGlove(leftGlove),
      rightGlove(rightGlove) {
}

std::array<bool, FINGER_COUNT> GlovePair::getLeftState() const {
    std::array<bool, FINGER_COUNT> state{};

    for (std::size_t i = 0; i < FINGER_COUNT; ++i) {
        state[i] = leftGlove.getFinger(i);
    }

    return state;
}

std::array<bool, FINGER_COUNT> GlovePair::getRightState() const {
    std::array<bool, FINGER_COUNT> state{};

    for (std::size_t i = 0; i < FINGER_COUNT; ++i) {
        state[i] = rightGlove.getFinger(i);
    }

    return state;
}

std::array<bool, PAIR_FINGER_COUNT> GlovePair::getState() const {
    std::array<bool, PAIR_FINGER_COUNT> state{};

    for (std::size_t i = 0; i < FINGER_COUNT; ++i) {
        state[i] = leftGlove.getFinger(i);
        state[i + FINGER_COUNT] = rightGlove.getFinger(i);
    }

    return state;
}

void GlovePair::setLeftState(
    const std::array<bool, FINGER_COUNT>& state) {
    leftGlove.setState(state);
}

void GlovePair::setRightState(
    const std::array<bool, FINGER_COUNT>& state) {
    rightGlove.setState(state);
}
