#include "test_support.hpp"
#include <transport/detail/state_codec.hpp>
#include <vector>

void test::stateCodec(Runner& runner) {
    using State = std::array<bool,FINGER_COUNT>;
    using Combined = std::array<bool,PAIR_FINGER_COUNT>;
    for (auto hand : {Hand::Left,Hand::Right}) {
        runner.run("StateCodec_ExactHand_" + show(hand), [=] {
            const State expected{true,false,true,false,false};
            const std::string wire = hand == Hand::Left ? "S5 L 10100" : "S5 R 10100";
            equal(serializeHandState({hand,expected}), wire);
            const auto decoded = parseHandState(wire);
            equal(decoded.hand,hand);
            equal(decoded.state,expected);
        });
        runner.run("StateCodec_AllHandPatternsRoundTrip_" + show(hand), [=] {
            for (unsigned mask=0; mask<32; ++mask) {
                State state{};
                for (std::size_t i=0; i<state.size(); ++i) state[i] = (mask & (1u<<i)) != 0;
                auto decoded = parseHandState(serializeHandState({hand,state}));
                equal(decoded.hand,hand);
                equal(decoded.state,state);
            }
        });
    }
    runner.run("StateCodec_ExactCombined", [] {
        const Combined state{true,false,true,false,false,false,false,false,true,false};
        equal(serializeState(state), "[1,0,1,0,0,0,0,0,1,0]");
        equal(parseState("[1,0,1,0,0,0,0,0,1,0]"),state);
    });
    runner.run("StateCodec_AllCombinedPatternsRoundTrip", [] {
        for (unsigned mask=0; mask<1024; ++mask) {
            Combined state{};
            for (std::size_t i=0; i<state.size(); ++i) state[i] = (mask & (1u<<i)) != 0;
            equal(parseState(serializeState(state)),state);
        }
    });
    runner.run("StateCodec_InvalidSerializedHand", [] {
        throws<std::invalid_argument>([] { (void)serializeHandState({static_cast<Hand>(9),{}}); });
    });
    const std::vector<std::string> invalidHands{
        "", "S5", "S5 L", "S5 L ", "S5 L 1010", "S5 L 101000", "S5 X 10100",
        "S5 l 10100", "S4 L 10100", "S5\tL 10100", "S5 L\t10100", "S5 L 10,00",
        "S5 L 10200", "S5 R 10x00", " S5 L 10100", "S5 L 10100 ", "S5 L 10100\n",
        "S5 R 10100\r", "S5 L 10100 extra", std::string("S5 L 10\0" "00",10)};
    for (const auto& input : invalidHands) {
        runner.run("StateCodec_InvalidHand_" + show(input), [=] {
            throws<std::invalid_argument>([&] { (void)parseHandState(input); });
        });
    }
    const std::vector<std::string> invalidStates{
        "", "[]", "[0,0,0,0,0]", "[0,0,0,0,0,0,0,0,0]", "[0,0,0,0,0,0,0,0,0,0,0]",
        "0,0,0,0,0,0,0,0,0,0", "(0,0,0,0,0,0,0,0,0,0)", "[2,0,0,0,0,0,0,0,0,0]",
        "[x,0,0,0,0,0,0,0,0,0]", "[0;0,0,0,0,0,0,0,0,0]", "[0 0,0,0,0,0,0,0,0,0]",
        "[0, 0,0,0,0,0,0,0,0,0]", " [0,0,0,0,0,0,0,0,0,0]", "[0,0,0,0,0,0,0,0,0,0]\n",
        "[0,0,0,0,0,0,0,0,0,0]x", "[0,0,0,0,0,0,0,0,0,0,]"};
    for (const auto& input : invalidStates) {
        runner.run("StateCodec_InvalidCombined_" + show(input), [=] {
            throws<std::invalid_argument>([&] { (void)parseState(input); });
        });
    }
}
