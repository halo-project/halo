#pragma once

#include "halo/tuner/TuningSection.h"
#include "halo/tuner/PseudoBayesTuner.h"
#include "halo/tuner/Bakeoff.h"
#include "halo/tuner/StatisticalStopper.h"

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

  // adjusts the Exploit factor based on the result of the bakeoff
  void adjustAfterBakeoff(Bakeoff::Result);

  PseudoBayesTuner PBT;
  StatisticalStopper Stopper;

  unsigned DuplicateCompilesInARow{0};
  uint64_t DuplicateCompiles{0};
  uint64_t TotalCompiles{0};

  // statistics for myself during development!!
  uint64_t Steps{0};
  uint64_t Bakeoffs{0};
  uint64_t SuccessfulBakeoffs{0};
  uint64_t BakeoffTimeouts{0};

  BakeoffParameters BP;
  llvm::Optional<Bakeoff> Bakery;
  ActivityState Status{ActivityState::Experiment};
  std::string BestLib;

  // for choosing a new code version
  const unsigned MAX_DUPES_IN_ROW; // max duplicate compiles in a row before we give up.
};

} // namespace halo