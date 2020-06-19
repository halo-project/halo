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
  PseudoBayesTuner(nlohmann::json const& Config, KnobSet const& BaseKnobs, std::unordered_map<std::string, CodeVersion> &Versions);

  // obtains a configuration that the tuner believes should be tried next
  // returns llvm::None if there's an error, or the tuner doesn't have
  // a sufficient prior established yet.
  llvm::Optional<KnobSet> getConfig();

private:
  KnobSet const& BaseKnobs;
  std::unordered_map<std::string, CodeVersion> &Versions;
  std::mt19937_64 RNG;

  // various hyperparameters of the tuner, which are initialized from the config file.
  // these affect the generateConfig process.
  size_t TopTaken;  // the top N predictions that will be saved to be tested for real.
  size_t SearchSz;  // the number of configurations to evaluate with the surrogate when generating new configs.
  const float HELDOUT_RATIO;  // (0,1) indicates how much of the dataset should be held-out during training, for validation purposes.
  float ExploreRatio; // [0,1] indicates how much to "explore", with the remaining percent used to "exploit".
  float EnergyLvl; // [0, 100] energy level to be used to perturb the best configuration when exploiting.

  std::list<KnobSet> GeneratedConfigs;

  // adds more configurations to the list of generated configs
  // using the pseudo-bayes tuner, based on the current state of
  // the code versions and their performance.
  llvm::Error generateConfigs();

  // Leveraging a model of the performance for configurations, we search the configuration-space
  // for new high-value configurations based on our prior experience.
  void surrogateSearch(std::vector<char> const& SerializedModel,
                  std::vector<std::pair<KnobSet const*, RandomQuantity const*>>& Prior,
                  size_t knobsPerConfig,
                  std::unordered_map<std::string, size_t> const& KnobToCol);

}; // end class

} // namespace halo
