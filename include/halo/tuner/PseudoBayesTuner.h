#pragma once

#include "halo/tuner/KnobSet.h"
#include "halo/tuner/CodeVersion.h"
#include "halo/tuner/ConfigManager.h"
#include "halo/nlohmann/json_fwd.hpp"
#include "llvm/Support/Error.h"
#include "llvm/ADT/Optional.h"
#include <random>
#include <list>


namespace halo {

class PseudoBayesTuner {
public:
  PseudoBayesTuner(nlohmann::json const& Config, KnobSet const& BaseKnobs, std::unordered_map<std::string, CodeVersion> &Versions);

  // obtains a configuration that the tuner believes should be tried next.
  KnobSet getConfig(std::string CurrentLib);

  // in case you need an RNG in a pinch, this one is seeded correctly!
  std::mt19937_64& getRNG() { return RNG; }

  // returns true iff the next config returned by getConfig has already been
  // determined, i.e., further profiling data / bakeoffs will not influence the
  // next config. This is useful if you'd like to get a head-start on
  // compiling configs.
  bool nextIsPredetermined() const { return Manager.sizeTop() > 0; }

  ConfigManager const& getConfigManager() const { return Manager; }

private:
  KnobSet const& BaseKnobs;
  std::unordered_map<std::string, CodeVersion> &Versions;
  std::mt19937_64 RNG;

  // various hyperparameters of the tuner, which are initialized from the config file.
  // these affect the generateConfig process.
  const size_t LearnIters; // number of iterations to learn in the model before stopping.
  const size_t TotalBatchSz;    // total number of configurations to generate every time we run out
  const size_t SearchSz;  // the number of configurations to evaluate with the surrogate when generating new configs.
  const size_t MIN_PRIOR; // the minimum number of <config,IPC> observations required in order to perform training.
  const float HELDOUT_RATIO;  // (0,1) indicates how much of the dataset should be held-out during training, for validation purposes.
  const float ExploreRatio; // [0,1] indicates how much to "explore", with the remaining percent used to "exploit".
  const float EnergyLvl; // [0, 100] energy level to be used to perturb the best configuration when exploiting.
  size_t ExploitBatchSz;  // the top N predictions that will be saved to be tested for real.

  ConfigManager Manager;

  // adds more configurations to the list of generated configs
  // using the pseudo-bayes tuner, based on the current state of
  // the code versions and their performance.
  llvm::Error generateConfigs(std::string CurrentLib);

  // Leveraging a model of the performance for configurations, we search the configuration-space
  // for new high-value configurations based on our prior experience.
  llvm::Error surrogateSearch(std::vector<char> const& SerializedModel,
                  CodeVersion const& bestVersion,
                  size_t knobsPerConfig,
                  std::unordered_map<std::string, size_t> const& KnobToCol);

}; // end class

} // namespace halo
