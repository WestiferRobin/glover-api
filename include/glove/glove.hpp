#ifndef GLOVE_HPP
#define GLOVE_HPP

#include <array>
#include <cstddef>

constexpr std::size_t FINGER_COUNT = 5;

enum class GloveRole {
    Primary,
    Secondary
};

enum class Hand {
    Left,
    Right
};

enum class Finger : std::size_t {
    Pinky = 0,
    Ring = 1,
    Middle = 2,
    Index = 3,
    Thumb = 4
};

class Glove {
private:
    std::array<bool, FINGER_COUNT> fingers{};
    GloveRole role;
    Hand hand;

    std::size_t fingerIndex(Finger finger) const;

public:
    Glove(Hand hand, GloveRole role);

    void setFinger(Finger finger, bool value);
    bool getFinger(Finger finger) const;

    void setState(const std::array<bool, FINGER_COUNT>& state);
    std::array<bool, FINGER_COUNT> getState() const;

    GloveRole getRole() const;
    Hand getHand() const;
};

#endif // GLOVE_HPP
