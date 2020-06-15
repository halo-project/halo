#pragma once

#include <string>
#include "llvm/ADT/Optional.h"
#include "halo/tuner/RandomQuantity.h"

namespace halo {

class TuningSection;
class GroupState;

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

  // makes progress on the bakeoff
  Result take_step(GroupState&);

private:

  static constexpr size_t SWITCH_RATE = 3;  // time steps until we switch versions.

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
};


} // end namespace