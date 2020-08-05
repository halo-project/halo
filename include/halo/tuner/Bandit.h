#pragma once

#include <set>
#include <map>
#include <random>
#include "Logging.h"

namespace halo {

/// A mathematical model (multi-armed bandit) for feedback-driven decision-making
template <typename Action>
class Bandit {
public:
    Bandit() {}

    virtual ~Bandit() {}

    /// Ask the model to take an action.
    virtual Action choose() = 0;

    /// Provide the model with a reward for the given action.
    virtual void reward(Action, float) = 0;

    virtual void dump(LoggingContext) const = 0;
};



template <typename Action>
class RecencyWeightedBandit : public Bandit<Action> {
public:
    /// @param actions  the options the bandit has to choose among
    ///
    /// @param stepSize must be in the range [0, 1], with the higher-end of
    ///                 the scale biasing towards more recently seen rewards,
    ///                 i.e., our opinion changes more strongly with newer info.
    ///                 If stepSize = 0.0, then recency weighting is disabled
    ///                 and all past rewards are used for the expectation, though
    ///                 this option should be limited only to stationary problems
    ///                 where convergence is possible.
    ///
    ///
    /// @param epsilon  represents the probability of choosing a random action
    ///                 instead of being greedy. Normally should be less than 0.1,
    ///                 meaning 10% chance of exploration instead of exploitation.
    ///                 Must be in the range [0, 1].
    ///
    /// @param seed     sets the internal random number generator's seed.
    RecencyWeightedBandit(std::set<Action> actions, float stepSize=0.1, float epsilon=0.1, uint32_t seed=475391234);

    Action choose() override;

    void reward(Action, float) override;

    void dump(LoggingContext LC=LC_Info) const override;

private:
    // The defaults of these members must be valid for actions that have
    // never been taken before.
    struct Metadata {
        float Expected = 0.0; // the expected reward for the given action, Q(A)
        unsigned NumOccurred = 0; // N(A)
    };

    std::vector<Action> Actions;
    float StepSize;
    float Epsilon;
    std::map<Action, Metadata> ActionValue;
    std::mt19937 RNG;
};

} // end namespace