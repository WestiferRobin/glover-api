#ifndef GLOVER_GLOVE_JSON_HPP
#define GLOVER_GLOVE_JSON_HPP
#include <transport/detail/state_codec.hpp>
#include <optional>

namespace glove_json {
struct Submission {
    GloveRole role;
    Hand hand;
    std::size_t index;
    bool combined;
    std::array<bool, FINGER_COUNT> fingers{}, left{}, right{};
};
Submission parse(const std::string& body);
std::string individual(GloveRole role, Hand hand, std::size_t index,
                       const std::array<bool, FINGER_COUNT>& fingers);
std::string combined(std::size_t index, const std::array<bool, PAIR_FINGER_COUNT>& state);
std::string combinedSubmission(Hand primaryHand, std::size_t index,
                               const std::array<bool, PAIR_FINGER_COUNT>& state);
bool isJsonContentType(std::string value);
std::optional<GloveRole> roleFilter(const std::string& target);
}
#endif
