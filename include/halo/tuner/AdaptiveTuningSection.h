#pragma once

#include "halo/tuner/TuningSection.h"
#include "halo/tuner/PseudoBayesTuner.h"
#include "halo/tuner/Bakeoff.h"
#include "halo/tuner/Actions.h"
#include "halo/tuner/Bandit.h"

namespace halo {

/// The main one with the fancy stuff.
class AdaptiveTuningSection : public TuningSection {
public:
  AdaptiveTuningSection(TuningSectionInitializer TSI, std::string RootFunc);
  void take_step(GroupState &) override;
  void dump() const override;

private:
  enum class ActivityState {
    Experiment,
    MakeDecision,
    Compiling,
    Bakeoff,
    Waiting
  };

  std::string stateToString(ActivityState S) const {
    switch(S) {
      case ActivityState::Experiment:         return "EXPERIMENT";
      case ActivityState::MakeDecision:       return "MAKE_DECISION";
      case ActivityState::Compiling:          return "COMPILING";
      case ActivityState::Bakeoff:            return "BAKEOFF";
      case ActivityState::Waiting:            return "WAITING";
    };
    return "?";
  }

  void transitionTo(ActivityState S);

  void transitionToBakeoff(GroupState &, std::string const&);
  void transitionToWait();
  void transitionToDecision(float Reward);

  // consumes the current bakery and returns a MAB reward.
  float computeReward();

  PseudoBayesTuner PBT;
  RecencyWeightedBandit<RootAction> MAB;
  const float BakeoffPenalty;
  const uint32_t StepsPerWaitAction;

  unsigned DuplicateCompilesInARow{0};
  uint64_t DuplicateCompiles{0};
  uint64_t TotalCompiles{0};
  uint32_t WaitStepsRemaining{0};

  // statistics for myself during development!!
  uint64_t Steps{0};
  uint64_t Bakeoffs{0};
  uint64_t SuccessfulBakeoffs{0};
  uint64_t BakeoffTimeouts{0};

  BakeoffParameters BP;
  llvm::Optional<Bakeoff> Bakery;
  ActivityState Status{ActivityState::Experiment};
  RootAction CurrentAction = RA_RunExperiment;
  std::string BestLib;

  // for choosing a new code version
  const unsigned MAX_DUPES_IN_ROW; // max duplicate compiles in a row before we give up.
  const unsigned RETRY_COIN_BIAS; // [1, 100]
};

} // namespace halo