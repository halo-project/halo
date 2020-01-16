#include "halo/tuner/Bandit.h"
#include "llvm/Support/Error.h"


namespace halo {

  template <typename Action>
  RecencyWeightedBandit<Action>::RecencyWeightedBandit(std::set<Action> actions, float stepSize)
    : Bandit<Action>(), Actions(actions), StepSize(stepSize) {
      if (StepSize < 0.0 || StepSize > 1.0)
        llvm::report_fatal_error("invalid step size.");
    }

  template <typename Action>
  Action RecencyWeightedBandit<Action>::choose() {
    // TODO: this should be:
    // 1. flip a biased coin
    //    a) most of the time, pick the action with highest expected reward
    //    b) pick a random action.

    return Actions[0];
  }

  template <typename Action>
  void RecencyWeightedBandit<Action>::reward(Action act, float newReward) {
    Metadata& MD = ActionValue[act];

    MD.NumOccurred += 1;

    float step;
    if (StepSize == 0.0)
      step = 1.0 / MD.NumOccurred;
    else
      step = StepSize;

    MD.Expected += step * (newReward - MD.Expected);
  }




}