#include <codec/glove_json.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <set>
#include <stdexcept>

namespace glove_json {
namespace {
using Json = nlohmann::json;
std::array<bool, FINGER_COUNT> fingers(const Json& value) {
    if (!value.is_array() || value.size() != FINGER_COUNT)
        throw std::invalid_argument("Expected five integer finger values");
    std::array<bool, FINGER_COUNT> result{};
    for (std::size_t i = 0; i < result.size(); ++i) {
        if (!value[i].is_number_integer() || (value[i] != 0 && value[i] != 1))
            throw std::invalid_argument("Finger values must be integers 0 or 1");
        result[i] = value[i] == 1;
    }
    return result;
}
Json bits(const std::array<bool, FINGER_COUNT>& state) {
    auto result = Json::array();
    for (bool bit : state) result.push_back(bit ? 1 : 0);
    return result;
}
std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t") - first + 1);
}
std::string decode(const std::string& value) {
    std::string result;
    auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%') {
            if (i + 2 >= value.size() || hex(value[i+1]) < 0 || hex(value[i+2]) < 0)
                throw std::invalid_argument("Malformed query encoding");
            result += static_cast<char>(hex(value[i+1]) * 16 + hex(value[i+2]));
            i += 2;
        } else result += value[i] == '+' ? ' ' : value[i];
    }
    return result;
}
}
Submission parse(const std::string& body) {
    try {
        std::set<std::string> keys;
        auto json = Json::parse(body, [&](int depth, Json::parse_event_t event, Json& value) {
            if (depth > 2) throw std::invalid_argument("Nested JSON is unsupported");
            if (event == Json::parse_event_t::key && !keys.insert(value.get<std::string>()).second)
                throw std::invalid_argument("Duplicate JSON key");
            return true;
        });
        if (!json.is_object()) throw std::invalid_argument("Expected a JSON object");
        const bool combined = !json.contains("fingers");
        const std::set<std::string> expected = combined
            ? std::set<std::string>{"role", "hand", "index", "left", "right"}
            : std::set<std::string>{"role", "hand", "index", "fingers"};
        if (keys != expected) throw std::invalid_argument("Missing, unknown or mixed fields");
        if (json["role"] != "primary" && json["role"] != "secondary")
            throw std::invalid_argument("Invalid role");
        if (json["hand"] != "left" && json["hand"] != "right")
            throw std::invalid_argument("Invalid physical hand");
        const auto& index = json["index"];
        if (!index.is_number_integer() || index < 0 || index > 4)
            throw std::invalid_argument("Index must be an integer from 0 to 4");
        Submission result{json["role"] == "primary" ? GloveRole::Primary : GloveRole::Secondary,
                          json["hand"] == "left" ? Hand::Left : Hand::Right,
                          index.get<std::size_t>(), combined};
        if (combined) {
            if (result.role != GloveRole::Primary)
                throw std::invalid_argument("Combined submissions require Primary role");
            result.left = fingers(json["left"]);
            result.right = fingers(json["right"]);
        } else result.fingers = fingers(json["fingers"]);
        return result;
    } catch (const Json::exception&) {
        throw std::invalid_argument("Invalid JSON submission");
    }
}
std::string individual(GloveRole role, Hand hand, std::size_t index,
                       const std::array<bool, FINGER_COUNT>& state) {
    return Json{{"role", role == GloveRole::Primary ? "primary" : "secondary"},
                {"hand", hand == Hand::Left ? "left" : "right"},
                {"index", index}, {"fingers", bits(state)}}.dump();
}
namespace {
Json combinedObject(std::size_t index, const std::array<bool, PAIR_FINGER_COUNT>& state) {
    std::array<bool, FINGER_COUNT> left{}, right{};
    std::copy_n(state.begin(), FINGER_COUNT, left.begin());
    std::copy_n(state.begin() + FINGER_COUNT, FINGER_COUNT, right.begin());
    return Json{{"index", index}, {"left", bits(left)}, {"right", bits(right)}};
}
}
std::string combined(std::size_t index, const std::array<bool, PAIR_FINGER_COUNT>& state) {
    return combinedObject(index, state).dump();
}
std::string combinedSubmission(Hand primaryHand, std::size_t index,
                               const std::array<bool, PAIR_FINGER_COUNT>& state) {
    if ((primaryHand != Hand::Left && primaryHand != Hand::Right) || index > 4)
        throw std::invalid_argument("Invalid combined submission hand or index");
    auto result = combinedObject(index, state);
    result["role"] = "primary";
    result["hand"] = primaryHand == Hand::Left ? "left" : "right";
    return result.dump();
}
bool isJsonContentType(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 'a' - 'A') : static_cast<char>(c);
    });
    const auto separator = value.find(';');
    if (trim(value.substr(0, separator)) != "application/json") return false;
    if (separator == std::string::npos) return true;
    auto parameter = trim(value.substr(separator + 1));
    const auto equals = parameter.find('=');
    if (equals == std::string::npos || trim(parameter.substr(0, equals)) != "charset") return false;
    auto charset = trim(parameter.substr(equals + 1));
    return charset == "utf-8" || charset == "\"utf-8\"";
}
std::optional<GloveRole> roleFilter(const std::string& target) {
    const auto question = target.find('?');
    if (question == std::string::npos) return {};
    const auto query = target.substr(question + 1);
    const auto equals = query.find('=');
    if (query.find('&') != std::string::npos || equals == std::string::npos ||
        decode(query.substr(0, equals)) != "role")
        throw std::invalid_argument("Expected exactly one role filter");
    const auto role = decode(query.substr(equals + 1));
    if (role == "primary") return GloveRole::Primary;
    if (role == "secondary") return GloveRole::Secondary;
    throw std::invalid_argument("Invalid role filter");
}
}
