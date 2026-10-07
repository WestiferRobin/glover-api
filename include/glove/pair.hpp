#ifndef GLOVE_PAIR_HPP
#define GLOVE_PAIR_HPP

#include <array>
#include <glove/glove.hpp>

constexpr std::size_t PAIR_FINGER_COUNT = FINGER_COUNT * 2;

class GlovePair {
private:
    Glove leftGlove;
    Glove rightGlove;

public:
    // Owns copies; later changes to the original gloves do not update this pair.
    // Requires one Dominant and one Supplementary glove (either hand may lead).
    // Throws std::invalid_argument for any other role combination.
    GlovePair(Glove leftGlove, Glove rightGlove);

    // Return value snapshots, not detected chords or synchronized sensor samples.
    std::array<bool, FINGER_COUNT> getLeftState() const;
    std::array<bool, FINGER_COUNT> getRightState() const;
    // Concatenates left then right, without reversal or role-dependent reordering:
    // L pinky, ring, middle, index, thumb; R thumb, index, middle, ring, pinky.
    std::array<bool, PAIR_FINGER_COUNT> getState() const;

    // Update the owned hand snapshots using the hand-specific index order.
    void setLeftState(const std::array<bool, FINGER_COUNT>& state);
    void setRightState(const std::array<bool, FINGER_COUNT>& state);
};

#endif // GLOVE_PAIR_HPP
