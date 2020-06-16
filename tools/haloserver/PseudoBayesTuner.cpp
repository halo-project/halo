#include "halo/tuner/PseudoBayesTuner.h"
#include "halo/nlohmann/util.hpp"

#include <cstdlib>
#include <cmath>

namespace halo {

PseudoBayesTuner::PseudoBayesTuner(nlohmann::json const& Config, std::unordered_map<std::string, CodeVersion> &Versions)
  : Versions(Versions),
    RNG(config::getServerSetting<uint64_t>("pbtuner-seed", Config)),
    HELDOUT_RATIO(config::getServerSetting<float>("pbtuner-heldout-ratio", Config))
  {}


// a _dense_ 2D array of configuration data, i.e.,
// cfg[rowNum][i]  must be written as  cfg[(rowNum * ncol) + i]
// each row corrsponds to one configuration.
// also includes a results vector of size = numberRows in the config matrix.
class ConfigMatrix {
public:
  using Array = std::unique_ptr<float, decltype(&std::free)>;

  ConfigMatrix(size_t numRows, size_t numCols)
    : numRows(numRows), numCols(numCols),
      cfg((float*) std::malloc(sizeof(float) * numRows * numCols), std::free),
      result((float*) std::malloc(sizeof(float) * numRows), std::free) {
        if (cfg == nullptr || result == nullptr)
          fatal_error("failed to allocate a config matrix of size " + std::to_string(numRows * numCols));
      }

  void setEntry(size_t rowNum, KnobSet const& Config, RandomQuantity const& IPC) {
    fatal_error("todo: write to the row!");
  }

private:
  size_t numRows;
  size_t numCols;
  Array cfg;
  Array result;
};


KnobSet PseudoBayesTuner::generateConfig() {
  if (Versions.size() == 0)
    fatal_error("cannot generate a config with no prior!");

  /////////////////////
  // establish prior

  size_t totalConfigs = 0;
  size_t knobsPerConfig = 0;
  { // we need to count how many configs and knobs we are working with.
    for (auto const& Entry : Versions) {

      // a code version is only usable if it has some observations.
      if (Entry.second.getIPC().size() == 0)
        continue;

      auto const& Configs = Entry.second.getConfigs();
      if (Configs.size() == 0)
        fatal_error("A code version is missing a knob configuration!");

      // make sure the knob sets have the same number of knobs.
      for (auto const& Config : Configs)
        if (knobsPerConfig == 0)
          knobsPerConfig = Config.size();
        else if (knobsPerConfig != Config.size())
          fatal_error("not every knobset has the same number of knobs?");

      totalConfigs += Configs.size();
    }
  }

  if (totalConfigs == 0)
    fatal_error("no usable configurations. please collect IPC measurements.");

  //////
  // split the data into training and validation sets

  size_t validateRows = std::max(1, (int) std::round(HELDOUT_RATIO * totalConfigs));
  size_t trainingRows = totalConfigs - validateRows;

  ConfigMatrix train(trainingRows, knobsPerConfig);
  ConfigMatrix validate(validateRows, knobsPerConfig);

  // TODO: dump the knob configurations to the matrices. we can just flip a coin when
  // we encounter each config.

  fatal_error("TODO");
}

} // end namespace