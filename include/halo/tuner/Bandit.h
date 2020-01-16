#pragma once

#include <set>
#include <map>

namespace halo {

/// A mathematical model (multi-armed bandit) for feedback-driven decision-making
template <typename Action>
class Bandit {
public:
    /// Initialize the bandit with the set of actions allowed
    Bandit() {}

    virtual ~Bandit();

    /// Ask the model to take an action.
    /// TODO: should this be made const?
    virtual Action choose() = 0;

    /// Provide the model with a reward for the given action.
    virtual void reward(Action, float) = 0;
};


template <typename Action>
class RecencyWeightedBandit {
public:
    /// @param actions  the options the bandit has to choose among
    ///
    /// @param stepSize must be in the range [0, 1], with the higher-end of
    ///                 the scale biasing towards more recently seen rewards,
    ///                 i.e., our opinion changes more strongly with newer info.
    ///                 If stepSize = 0.0, then recency weighting is disabled
    ///                 and all past rewards are used for the expectation.
    RecencyWeightedBandit(std::set<Action> actions, float stepSize);

    Action choose() override;

    void reward(Action, float) override;

private:
    // The defaults of these members must be valid for actions that have
    // never been taken before.
    struct Metadata {
        float Expected = 0.0; // the expected reward for the given action, Q(A)
        unsigned NumOccurred = 0; // N(A)
    };

    std::set<Action> Actions;
    float StepSize;
    std::map<Action, Metadata> ActionValue;
};

} // end namespace