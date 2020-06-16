#pragma once

#include <string>
#include "llvm/ADT/Optional.h"
#include "halo/tuner/RandomQuantity.h"

namespace halo {

class TuningSection;
class GroupState;


/// an online comparison of two versions of the same code.
/// The term 'bakeoff' and its general design here is based on
/// work by:
/// Lau et al in PLDI'06, "Online Performance Auditing: Using Hot Optimizations Without Getting Burned"
class Bakeoff {
public:
  Bakeoff(GroupState &, TuningSection *TS, std::string Current, std::string New);

  enum class Result {
    Finished,
    Timeout,
    InProgress
  };

  // if the bakeoff has finished, then this function declares the winner.
  llvm::Optional<std::string> getWinner() const;

  // returns the name of the library that's currently deployed in this bake-off
  std::string getDeployed() const { return Deployed; }

  // the NOT deployed one.
  std::string getOther() const { return Other; }

  // makes progress on the bakeoff
  Result take_step(GroupState&);

private:

  static constexpr size_t SWITCH_RATE = 2;  // time steps until we switch versions.
  static constexpr size_t MAX_SWITCHES = 8; // the timeout threshold.

  void deploy(GroupState&, std::string const& LibName);
  void switchVersions(GroupState&);

  bool hasSufficientObservations(std::string const& LibName);

  enum class ComparisonResult {
    LessThan,
    GreaterThan,
    NoAnswer
  };

  ComparisonResult compare_ttest(RandomQuantity const& A, RandomQuantity const& B) const;

  ComparisonResult compare_means(RandomQuantity const& A, RandomQuantity const& B) const;

  TuningSection *TS;
  std::string Deployed;
  std::string Other;

  llvm::Optional<std::string> Winner;
  Result Status;
  size_t DeployedSampledSeen{0};
  size_t StepsUntilSwitch{SWITCH_RATE};
  size_t Switches{0};
};


} // end namespace