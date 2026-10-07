#include <iostream>
#include <array>

#include <glove/glove.hpp>
#include <glove/pair.hpp>

void printState(const std::array<bool, PAIR_FINGER_COUNT>& state) {
    std::cout << "[";

    for (std::size_t i = 0; i < state.size(); ++i) {
        std::cout << state[i];

        if (i < state.size() - 1) {
            std::cout << ",";
        }
    }

    std::cout << "]" << std::endl;
}

GlovePair initializeGloves() {
    Glove rightGlove(GloveRole::Dominant);
    Glove leftGlove(GloveRole::Supplementary);

    leftGlove.setState({
        true, false, true, false, true
    });

    rightGlove.setState({
        false, true, false, true, false
    });

    return GlovePair(leftGlove, rightGlove);
}

int main() {
    GlovePair gloves = initializeGloves();

    // "I"
    gloves.setLeftState({
        true, false, false, false, false
    });

    gloves.setRightState({
        false, false, false, false, true
    });

    printState(gloves.getState());

    // "you"
    gloves.setLeftState({
        false, true, false, false, false
    });

    gloves.setRightState({
        false, false, false, true, false
    });

    printState(gloves.getState());

    // "we"
    gloves.setLeftState({
        false, false, true, false, false
    });

    gloves.setRightState({
        false, false, true, false, false
    });

    printState(gloves.getState());

    return 0;
}