#pragma once

#include "halo/tuner/KnobSet.h"
#include "halo/tuner/CodeVersion.h"
#include "halo/nlohmann/json_fwd.hpp"
#include "llvm/Support/Error.h"
#include "llvm/ADT/Optional.h"
#include <random>
#include <list>


namespace halo {

class PseudoBayesTuner {
public:
  PseudoBayesTuner(nlohmann::json const& Config, std::unordered_map<std::string, CodeVersion> &Versions);

  // obtains a configuration that the tuner believes should be tried next
  // returns llvm::None if there's an error, or the tuner doesn't have
  // a sufficient prior established yet.
  llvm::Optional<KnobSet> getConfig();

private:
  std::unordered_map<std::string, CodeVersion> &Versions;
  std::mt19937_64 RNG;

  const float HELDOUT_RATIO;

  std::list<KnobSet> GeneratedConfigs;

  // adds more configurations to the list of generated configs
  // using the pseudo-bayes tuner, based on the current state of
  // the code versions and their performance.
  llvm::Error generateConfigs();

}; // end class

} // namespace halo
