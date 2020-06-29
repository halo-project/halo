#pragma once

#include <string>
#include "llvm/ADT/Optional.h"
#include "halo/tuner/RandomQuantity.h"
#include "halo/nlohmann/json_fwd.hpp"

namespace halo {

class TuningSection;
class GroupState;
class TSPerf;

struct BakeoffParameters {
  BakeoffParameters(nlohmann::json const& Config);

  size_t SWITCH_RATE;  // time steps until we switch versions.
  size_t MAX_SWITCHES; // the timeout threshold.
  size_t MIN_SAMPLES;
  float CONFIDENCE;    // must be a float constant like 0.95f, with f suffix
};

/// an online comparison of two versions of the same code.
/// The term 'bakeoff' and its general design here is based on
/// work by:
/// Lau et al in PLDI'06, "Online Performance Auditing: Using Hot Optimizations Without Getting Burned"
class Bakeoff {
public:
  Bakeoff(GroupState &, TuningSection *TS, BakeoffParameters BP, std::string Current, std::string New);

  enum class Result {
    CurrentIsBetter,  // finished
    NewIsBetter,      // finished
    Timeout,          // finished (but no winner!)
    InProgress
  };

  // if the bakeoff has finished, then this function returns the winner.
  llvm::Optional<std::string> getWinner() const;

  Result lastResult() const { return Status; }

  // returns the name of the library that's currently deployed in this bake-off
  std::string getDeployed() const { return Deployed; }

  // the NOT deployed one.
  std::string getOther() const { return Other; }

  // makes progress on the bakeoff
  Result take_step(GroupState&);

  void dump() const;

private:

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

  BakeoffParameters BP;
  TuningSection *TS;

  std::vector<TSPerf> History;

  // name of the "new" library we're suppose to be testing
  std::string NEW_LIBNAME;

  // manages the switches between libs
  std::string Deployed;
  std::string Other;

  llvm::Optional<std::string> Winner;
  Result Status;
  size_t DeployedSampledSeen{0};
  size_t StepsUntilSwitch;
  size_t Switches{0};
};


} // end namespace