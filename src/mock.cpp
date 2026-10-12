#include <mock.hpp>

#include <iostream>

const std::array<MockSample, 5>& mockSamples() {
    static const std::array<MockSample, 5> samples{{
        {{true, false, false, false, false}, {false, false, false, false, true}},
        {{false, true, false, false, false}, {false, false, false, true, false}},
        {{false, false, true, false, false}, {false, false, true, false, false}},
        {{true, false, true, false, false}, {false, false, false, true, false}},
        {{true, true, true, true, true}, {true, true, true, true, true}}
    }};
    return samples;
}

void printMockState(std::ostream& output,
                    const std::array<bool, PAIR_FINGER_COUNT>& state) {
    output << '[';
    for (std::size_t finger = 0; finger < state.size(); ++finger) {
        if (finger != 0) {
            output << ',';
        }
        output << (state[finger] ? '1' : '0');
    }
    output << "]\n";
}

void runMock() {
    Glove leftGlove(Hand::Left, GloveRole::Secondary);
    Glove rightGlove(Hand::Right, GloveRole::Primary);
    GlovePair gloves(leftGlove, rightGlove);

    for (const auto& sample : mockSamples()) {
        gloves.setLeftState(sample.left);
        gloves.setRightState(sample.right);
        printMockState(std::cout, gloves.getState());
    }
}
