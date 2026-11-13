#include "test_support.hpp"
#include <glove/glove.hpp>
#include <limits>

void test::glove(Runner& runner) {
    using State = std::array<bool, FINGER_COUNT>;
    for (Hand hand : {Hand::Left, Hand::Right}) {
        for (GloveRole role : {GloveRole::Primary, GloveRole::Secondary}) {
            runner.run("Glove_Construction_" + show(hand) + "_" + show(role), [=] {
                Glove glove(hand, role);
                equal(glove.getHand(), hand);
                equal(glove.getRole(), role);
                equal(glove.getState(), State{});
            });
        }
        for (std::size_t finger = 0; finger < FINGER_COUNT; ++finger) {
            runner.run("Glove_AnatomicalFinger_" + show(hand) + "_" + show(finger), [=] {
                Glove glove(hand, GloveRole::Primary);
                glove.setFinger(static_cast<Finger>(finger), true);
                State expected{};
                expected[hand == Hand::Left ? finger : 4 - finger] = true;
                equal(glove.getState(), expected);
                for (std::size_t other = 0; other < FINGER_COUNT; ++other)
                    equal(glove.getFinger(static_cast<Finger>(other)), other == finger);
                glove.setFinger(static_cast<Finger>(finger), false);
                equal(glove.getState(), State{});
            });
        }
        runner.run("Glove_StateReplacementAndCopies_" + show(hand), [=] {
            Glove glove(hand, GloveRole::Secondary);
            State input{true, false, false, true, false};
            glove.setState(input);
            input.fill(false);
            equal(glove.getState(), (State{true, false, false, true, false}));
            auto snapshot = glove.getState();
            snapshot.fill(true);
            equal(glove.getState(), (State{true, false, false, true, false}));
            Glove copy = glove;
            copy.setState(State{});
            equal(glove.getState(), (State{true, false, false, true, false}));
            glove.setState(State{false, true, false, false, true});
            equal(glove.getState(), (State{false, true, false, false, true}));
            for (std::size_t i = 0; i < FINGER_COUNT; ++i) {
                const State expected{false, true, false, false, true};
                equal(glove.getFinger(static_cast<Finger>(i)), expected[hand == Hand::Left ? i : 4-i]);
            }
        });
        for (auto invalid : {FINGER_COUNT, std::numeric_limits<std::size_t>::max()}) {
            runner.run("Glove_InvalidFinger_" + show(hand) + "_" + show(invalid), [=] {
                Glove glove(hand, GloveRole::Primary);
                throws<std::out_of_range>([&] { glove.setFinger(static_cast<Finger>(invalid), true); });
                throws<std::out_of_range>([&] { (void)glove.getFinger(static_cast<Finger>(invalid)); });
                equal(glove.getState(), State{});
            });
        }
    }
    for (int invalid : {-1, 2, 99}) {
        runner.run("Glove_InvalidHand_" + show(invalid), [=] {
            throws<std::invalid_argument>([=] { Glove glove(static_cast<Hand>(invalid), GloveRole::Primary); });
        });
        runner.run("Glove_InvalidRole_" + show(invalid), [=] {
            throws<std::invalid_argument>([=] { Glove glove(Hand::Left, static_cast<GloveRole>(invalid)); });
        });
    }
}
