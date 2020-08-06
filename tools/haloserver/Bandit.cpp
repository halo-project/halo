#include "halo/tuner/Actions.h"
#include "halo/tuner/Bandit.h"
#include "llvm/Support/Error.h"

#include "Logging.h"


namespace halo {

  std::string actionAsString(RootAction A) {
    switch (A) {
      case RootAction::RA_RunExperiment:  return "Experiment";
      case RootAction::RA_RetryBest:      return "Retry";
      case RootAction::RA_Wait:           return "Wait";
    }
    fatal_error("unknown action in actionAsString");
  }

  // NOTE: ideas for tweaking based on existing work include:
  //
  // 1. Have epsilon decrease over time, and start with a higher initial value.
  //

  template <typename Action>
  RecencyWeightedBandit<Action>::RecencyWeightedBandit(std::set<Action> actions, std::mt19937_64& generator, float stepSize, float epsilon)
    : Bandit<Action>(), Actions(actions.begin(), actions.end()), StepSize(stepSize), Epsilon(epsilon), RNG(generator) {
      if (StepSize < 0.0 || StepSize > 1.0)
        fatal_error("invalid step size.");

      if (Epsilon < 0.0 || Epsilon > 1.0)
        fatal_error("invalid probability for epsilon.");
    }


  template <typename Action>
  void RecencyWeightedBandit<Action>::setActionValues(std::map<Action, float> const& Init) {
    for (auto const& Entry : Init)
      ActionValue[Entry.first].Expected = Entry.second;
  }


  template <typename Action>
  Action RecencyWeightedBandit<Action>::choose() {

    std::uniform_real_distribution<float> dice(0.0, 1.0);
    std::uniform_int_distribution<int> actionDice(0, Actions.size()-1);

    // determine whether we should explore this time.
    if (dice(RNG) < Epsilon) {
      // then we should explore. we choose a uniformly random action.
      return Actions[actionDice(RNG)];
    }

    // otherwise we are making the greedy choice
    // by choosing the best known action.

    // make sure first-ever choice is random
    Action bestAction = Actions[actionDice(RNG)];
    float bestReward = ActionValue[bestAction].Expected;

    for (auto const& Entry : ActionValue) {
      Action Act = Entry.first;
      Metadata MD = Entry.second;
      bool TakeThisOne = false;

      if (MD.Expected > bestReward) {
        TakeThisOne = true;
      } else if (MD.Expected == bestReward) {
        // break tie randomly.
        std::uniform_int_distribution<int> coin(0, 1);
        TakeThisOne = (bool) coin(RNG);
      }

      if (TakeThisOne) {
        bestAction = Act;
        bestReward = MD.Expected;
      }

    }

    return bestAction;
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

  template <typename Action>
  void RecencyWeightedBandit<Action>::dump(LoggingContext LC) const {
      for (auto const& Entry : ActionValue) {
        clogs(LC) << "\t" << actionAsString(Entry.first)
                  << " x " << Entry.second.NumOccurred
                  << " --> " << Entry.second.Expected << "\n";
      }
  }


/////////
// Manual instantiations of template bandits because c++ is lame.

template class RecencyWeightedBandit<RootAction>;

} // end namespace