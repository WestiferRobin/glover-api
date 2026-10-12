#ifndef GLOVER_MOCK_HPP
#define GLOVER_MOCK_HPP

#include <glove/pair.hpp>
#include <iosfwd>

struct MockSample {
    std::array<bool, FINGER_COUNT> left;
    std::array<bool, FINGER_COUNT> right;
};

const std::array<MockSample, 5>& mockSamples();
void printMockState(std::ostream& output,
                    const std::array<bool, PAIR_FINGER_COUNT>& state);

// Emit the five original logical state snapshots to stdout.
void runMock();

#endif // GLOVER_MOCK_HPP
