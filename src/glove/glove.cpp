#include <glove/glove.hpp>

#include <stdexcept>

Glove::Glove(GloveRole role) : role(role) {
    for (std::size_t i = 0; i < FINGER_COUNT; ++i) {
        fingers[i] = false;
    }
}

void Glove::setFinger(std::size_t finger, bool value) {
    if (finger >= FINGER_COUNT) {
        throw std::out_of_range("Finger index out of range");
    }

    fingers[finger] = value;
}

bool Glove::getFinger(std::size_t finger) const {
    if (finger >= FINGER_COUNT) {
        throw std::out_of_range("Finger index out of range");
    }

    return fingers[finger];
}

void Glove::setState(const std::array<bool, FINGER_COUNT>& state) {
    for (std::size_t i = 0; i < FINGER_COUNT; ++i) {
        fingers[i] = state[i];
    }
}

GloveRole Glove::getRole() const {
    return role;
}