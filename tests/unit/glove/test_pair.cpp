#include "test_support.hpp"
#include <glove/pair.hpp>

void test::pair(Runner& runner) {
    using State = std::array<bool, FINGER_COUNT>;
    using Combined = std::array<bool, PAIR_FINGER_COUNT>;
    for (GloveRole leftRole : {GloveRole::Primary, GloveRole::Secondary}) {
        const auto rightRole = leftRole == GloveRole::Primary ? GloveRole::Secondary : GloveRole::Primary;
        runner.run("GlovePair_LeftFirstRegardlessOfRole_" + show(leftRole), [=] {
            Glove left(Hand::Left, leftRole), right(Hand::Right, rightRole);
            left.setFinger(Finger::Pinky, true);
            left.setFinger(Finger::Index, true);
            right.setFinger(Finger::Thumb, true);
            right.setFinger(Finger::Ring, true);
            GlovePair pair(left, right);
            equal(pair.getLeftState(), (State{true,false,false,true,false}));
            equal(pair.getRightState(), (State{true,false,false,true,false}));
            equal(pair.getState(), (Combined{true,false,false,true,false,true,false,false,true,false}));
            left.setState(State{});
            right.setState(State{});
            equal(pair.getState(), (Combined{true,false,false,true,false,true,false,false,true,false}));
        });
        runner.run("GlovePair_IndependentUpdatesAndSnapshots_" + show(leftRole), [=] {
            GlovePair pair(Glove(Hand::Left,leftRole), Glove(Hand::Right,rightRole));
            equal(pair.getState(), Combined{});
            pair.setLeftState(State{true,false,true,false,false});
            equal(pair.getRightState(), State{});
            pair.setRightState(State{false,true,false,false,true});
            const Combined expected{true,false,true,false,false,false,true,false,false,true};
            equal(pair.getState(), expected);
            auto snapshot = pair.getState();
            snapshot.fill(false);
            auto left = pair.getLeftState(); left.fill(false);
            auto right = pair.getRightState(); right.fill(false);
            equal(pair.getState(), expected);
            auto copy = pair;
            copy.setLeftState(State{});
            equal(pair.getState(), expected);
            pair.setLeftState(State{});
            equal(pair.getRightState(), (State{false,true,false,false,true}));
            pair.setRightState(State{});
            equal(pair.getState(), Combined{});
        });
    }
    for (auto hands : {std::pair{Hand::Left,Hand::Left}, std::pair{Hand::Right,Hand::Right},
                       std::pair{Hand::Right,Hand::Left}}) {
        runner.run("GlovePair_InvalidHands_" + show(hands.first) + "_" + show(hands.second), [=] {
            throws<std::invalid_argument>([=] {
                GlovePair pair(Glove(hands.first,GloveRole::Primary), Glove(hands.second,GloveRole::Secondary));
            });
        });
    }
    for (GloveRole role : {GloveRole::Primary, GloveRole::Secondary}) {
        runner.run("GlovePair_RejectsSameRole_" + show(role), [=] {
            throws<std::invalid_argument>([=] {
                GlovePair pair(Glove(Hand::Left,role), Glove(Hand::Right,role));
            });
        });
    }
}
