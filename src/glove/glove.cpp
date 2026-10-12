#include <glove/glove.hpp>

#include <stdexcept>

Glove::Glove(Hand hand, GloveRole role)
    : role(role),
      hand(hand) {

    if (hand != Hand::Left && hand != Hand::Right) {
        throw std::invalid_argument("Invalid hand");
    }

    if (role != GloveRole::Primary &&
        role != GloveRole::Secondary) {
        throw std::invalid_argument("Invalid glove role");
    }
}

std::size_t Glove::fingerIndex(Finger finger) const {
    const auto index = static_cast<std::size_t>(finger);

    if (index >= FINGER_COUNT) {
        throw std::out_of_range("Finger index out of range");
    }

    return hand == Hand::Left
        ? index
        : FINGER_COUNT - 1 - index;
}

void Glove::setFinger(Finger finger, bool value) {
    fingers[fingerIndex(finger)] = value;
}

bool Glove::getFinger(Finger finger) const {
    return fingers[fingerIndex(finger)];
}

void Glove::setState(
    const std::array<bool, FINGER_COUNT>& state) {
    fingers = state;
}

std::array<bool, FINGER_COUNT> Glove::getState() const {
    return fingers;
}

GloveRole Glove::getRole() const {
    return role;
}

Hand Glove::getHand() const {
    return hand;
}
