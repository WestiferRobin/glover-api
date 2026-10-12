#ifndef GLOVER_STATE_CODEC_HPP
#define GLOVER_STATE_CODEC_HPP
#include <glove/pair.hpp>
#include <string>
#include <string_view>

// Payload only: callers append LF when sending; TCP removes LF when receiving.
std::string serializeState(const std::array<bool, PAIR_FINGER_COUNT>& state);
std::array<bool, PAIR_FINGER_COUNT> parseState(std::string_view payload);
struct HandState {
    Hand hand;
    std::array<bool, FINGER_COUNT> state;
};
std::string serializeHandState(const HandState& sample);
HandState parseHandState(std::string_view payload);
#endif
