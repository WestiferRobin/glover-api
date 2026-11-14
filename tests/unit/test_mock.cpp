#include "test_support.hpp"
#include <mock.hpp>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
std::string oracle(const std::filesystem::path& directory) {
    std::ifstream input(directory / "expected_mock.txt", std::ios::binary);
    test::check(input.is_open(), "open independent mock oracle");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
// Deliberately does not call a production codec to interpret expected values.
std::vector<std::array<bool, PAIR_FINGER_COUNT>> expectedStates(const std::string& text) {
    test::check(!text.empty() && text.back() == '\n', "oracle ends with LF");
    std::istringstream input(text);
    std::vector<std::array<bool, PAIR_FINGER_COUNT>> states;
    std::string line;
    while (std::getline(input, line)) {
        test::equal(line.size(), std::size_t{21});
        test::equal(line.front(), '[');
        test::equal(line.back(), ']');
        std::array<bool, PAIR_FINGER_COUNT> state{};
        for (std::size_t i = 0; i < state.size(); ++i) {
            test::check(line[1+2*i] == '0' || line[1+2*i] == '1', "oracle bit is binary");
            state[i] = line[1+2*i] == '1';
            if (i != state.size()-1) test::equal(line[2+2*i], ',');
        }
        states.push_back(state);
    }
    test::equal(states.size(), std::size_t{5});
    return states;
}
}

void test::mock(Runner& runner) {
    runner.run("Mock_CanonicalValuesAndSequence", [&] {
        const auto states = expectedStates(oracle(runner.fixtures));
        const auto& samples = mockSamples();
        equal(samples.size(), std::size_t{5});
        for (std::size_t i = 0; i < samples.size(); ++i) {
            for (std::size_t finger = 0; finger < FINGER_COUNT; ++finger) {
                equal(samples[i].left[finger], states[i][finger]);
                equal(samples[i].right[finger], states[i][finger+FINGER_COUNT]);
            }
        }
    });
    for (auto leftRole : {GloveRole::Primary, GloveRole::Secondary}) {
        runner.run("Mock_ExactOutputBothAssignments_" + show(leftRole), [&runner,leftRole] {
            const auto text = oracle(runner.fixtures);
            const auto states = expectedStates(text);
            GlovePair pair(Glove(Hand::Left,leftRole), Glove(Hand::Right,
                leftRole == GloveRole::Primary ? GloveRole::Secondary : GloveRole::Primary));
            std::ostringstream output;
            std::size_t i = 0;
            for (const auto& sample : mockSamples()) {
                pair.setLeftState(sample.left);
                pair.setRightState(sample.right);
                equal(pair.getState(), states.at(i++));
                printMockState(output, pair.getState());
            }
            equal(output.str(), text);
        });
    }
    runner.run("Mock_RunMockExactOutput", [&] {
        const auto expected = oracle(runner.fixtures);
        std::ostringstream output;
        // Restore cout even if runMock throws; do not redirect runner output.
        struct Restore {
            std::streambuf* original;
            ~Restore() { std::cout.rdbuf(original); }
        };
        {
            Restore restore{std::cout.rdbuf(output.rdbuf())};
            runMock();
        }
        equal(output.str(), expected);
    });
}
