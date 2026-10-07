#ifndef GLOVE_HPP
#define GLOVE_HPP

#include <array>
#include <cstddef>

constexpr std::size_t FINGER_COUNT = 5;

// Communication responsibility, independent of physical hand identity.
enum class GloveRole {
    Dominant,
    Supplementary
};

// One physical glove with five logical states: false = inactive, true = active.
// Index order depends on the hand supplied by the caller:
// left: pinky, ring, middle, index, thumb; right: thumb, index, middle, ring, pinky.
// This logical representation is not a wireless packet format.
class Glove {
private:
    bool fingers[FINGER_COUNT];
    GloveRole role;

public:
    // Initializes all fingers inactive.
    Glove(GloveRole role);

    // Finger indexes are 0..4; invalid indexes throw std::out_of_range.
    void setFinger(std::size_t finger, bool value);
    // Returns the current logical value, not a detected chord.
    bool getFinger(std::size_t finger) const;

    void setState(const std::array<bool, FINGER_COUNT>& state);

    GloveRole getRole() const;
};

#endif // GLOVE_HPP
