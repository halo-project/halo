#pragma once

#include "halo/tuner/TuningSection.h"
#include "halo/tuner/PseudoBayesTuner.h"
#include "halo/tuner/Bakeoff.h"

namespace halo {

/// haven't decided on what this should do yet.
class AggressiveTuningSection : public TuningSection {
public:
  AggressiveTuningSection(TuningSectionInitializer TSI, std::string RootFunc);
  void take_step(GroupState &) override;
  void dump() const override;

private:
  enum class ActivityState {
    Ready,
    Paused,   // basically, exploiting
    WaitingForCompile,
    TestingNewLib
  };

  std::string stateToString(ActivityState S) const {
    switch(S) {
      case ActivityState::Ready:                return "READY";
      case ActivityState::Paused:               return "PAUSED";
      case ActivityState::WaitingForCompile:    return "COMPILING";
      case ActivityState::TestingNewLib:        return "BAKEOFF";
    };
    return "?";
  }

  void transitionTo(ActivityState S);

  // adjusts the Exploit factor based on the result of the bakeoff
  void adjustAfterBakeoff(Bakeoff::Result);

  PseudoBayesTuner PBT;

  uint64_t ExploitSteps{1}; // the count-down for steps remaining to perform that "exploit"
  size_t SamplesLastTime{0};
  unsigned DuplicateCompilesInARow{0};

  // statistics for myself during development!!
  uint64_t Steps{0};
  uint64_t Bakeoffs{0};
  uint64_t SuccessfulBakeoffs{0};
  uint64_t BakeoffTimeouts{0};
  uint64_t DuplicateCompiles{0};

  BakeoffParameters BP;
  llvm::Optional<Bakeoff> Bakery;
  ActivityState Status{ActivityState::Ready};
  std::string BestLib;

  // for choosing a new code version
  const unsigned MAX_DUPES_IN_ROW; // max duplicate compiles in a row before we give up.

  // for managing explore-exploit tradeoff
  const float EXPLOIT_LEARNING_RATE;
  const float MAX_TGT_FACTOR;
  const float MIN_TGT_FACTOR;
  float ExploitFactor; // the number of steps to be taken in the next "exploit" phase
};

} // namespace halo