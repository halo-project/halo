#pragma once

#include "halo/tuner/KnobSet.h"
#include "halo/tuner/CodeVersion.h"
#include <unordered_map>

namespace halo {

// Based on work by:
//
// Vuduc, Richard, Jeff Bilmes, and James Demmel. 2000.
//    “Statistical Modeling of Feedback Data in an Automatic Tuning System.”
//    In MICRO-33: Third ACM Workshop on Feedback-Directed Dynamic Optimization.
//
// Vuduc, Richard, James W. Demmel, and Jeff A. Bilmes. 2004.
//    “Statistical Models for Empirical Search-Based Performance Tuning.”
//    The International Journal of High Performance Computing Applications 18 (1): 65–94. https://doi.org/10.1177/1094342004041293.
class StatisticalStopper {
public:
  using FloatTy = double;

  StatisticalStopper(KnobSet const& ConfigSpace, FloatTy alpha=0.1, FloatTy epsilon=0.05)
    : Alpha(alpha), Epsilon(epsilon), N(ConfigSpace.cardinality()) {}

  bool shouldStop(std::string const& BestLib, std::unordered_map<std::string, CodeVersion> const&) const;

private:
  // [0, 1], the probability of error the user will tolerate
  // e.g., alpha = 0.1 --> "probability of error 10%"
  const FloatTy Alpha;

  // [0, 1], how faw away from the best implementation the user will tolerate.
  // e.g., epsilon = 0.05 --> "within 5% of the best implenentation"
  const FloatTy Epsilon;

  // the total number of configurations possible
  const FloatTy N;
};

} // namespace halo