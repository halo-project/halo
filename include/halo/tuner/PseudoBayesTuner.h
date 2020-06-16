#pragma once

#include "halo/tuner/KnobSet.h"
#include "halo/tuner/CodeVersion.h"
#include "halo/nlohmann/json_fwd.hpp"
#include <xgboost/c_api.h>
#include <random>


namespace halo {

class PseudoBayesTuner {
public:
  PseudoBayesTuner(nlohmann::json const& Config, std::unordered_map<std::string, CodeVersion> &Versions);

  // obtains a configuration that the tuner believes should be tried next
  KnobSet generateConfig();

private:
  std::unordered_map<std::string, CodeVersion> &Versions;
  std::mt19937_64 RNG;

  const float HELDOUT_RATIO;

}; // end class

} // namespace halo
