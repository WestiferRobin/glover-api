#include <glove/pair.hpp>

#include <stdexcept>

GlovePair::GlovePair(Glove leftGlove, Glove rightGlove)
    : leftGlove(leftGlove),
      rightGlove(rightGlove) {

    if (leftGlove.getHand() != Hand::Left ||
        rightGlove.getHand() != Hand::Right) {
        throw std::invalid_argument(
            "GlovePair requires left and right gloves");
    }

    if (leftGlove.getRole() == rightGlove.getRole()) {
        throw std::invalid_argument(
            "GlovePair requires one Primary and one Secondary glove");
    }
}

std::array<bool, FINGER_COUNT> GlovePair::getLeftState() const {
    return leftGlove.getState();
}

std::array<bool, FINGER_COUNT> GlovePair::getRightState() const {
    return rightGlove.getState();
}

std::array<bool, PAIR_FINGER_COUNT> GlovePair::getState() const {
    std::array<bool, PAIR_FINGER_COUNT> state{};

    const auto left = leftGlove.getState();
    const auto right = rightGlove.getState();

    for (std::size_t i = 0; i < FINGER_COUNT; ++i) {
        state[i] = left[i];
        state[i + FINGER_COUNT] = right[i];
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
