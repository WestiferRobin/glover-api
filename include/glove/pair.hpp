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
    GlovePair(Glove leftGlove, Glove rightGlove);

    std::array<bool, FINGER_COUNT> getLeftState() const;
    std::array<bool, FINGER_COUNT> getRightState() const;
    std::array<bool, PAIR_FINGER_COUNT> getState() const;

    void setLeftState(const std::array<bool, FINGER_COUNT>& state);
    void setRightState(const std::array<bool, FINGER_COUNT>& state);
};

#endif // GLOVE_PAIR_HPP