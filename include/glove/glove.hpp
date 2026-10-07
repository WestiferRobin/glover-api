#ifndef GLOVE_HPP
#define GLOVE_HPP

#include <array>
#include <cstddef>

constexpr std::size_t FINGER_COUNT = 5;

enum class GloveRole {
    Dominant,
    Supplementary
};

class Glove {
private:
    bool fingers[FINGER_COUNT];
    GloveRole role;

public:
    Glove(GloveRole role);

    void setFinger(std::size_t finger, bool value);
    bool getFinger(std::size_t finger) const;

    void setState(const std::array<bool, FINGER_COUNT>& state);

    GloveRole getRole() const;
};

#endif // GLOVE_HPP