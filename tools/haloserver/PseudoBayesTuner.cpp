#include "halo/tuner/PseudoBayesTuner.h"
#include "halo/nlohmann/util.hpp"

#include <cstdlib>
#include <cmath>
#include <algorithm>

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
  using FloatTy = float;
  using Array = std::unique_ptr<FloatTy, decltype(&std::free)>;
  static constexpr FloatTy MISSING_VAL = std::numeric_limits<FloatTy>::quiet_NaN();

  ConfigMatrix(size_t numRows, size_t numCols, std::unordered_map<std::string, size_t> const& KnobToCol)
    : nextFree(0), NUM_ROWS(numRows), NUM_COLS(numCols), KnobToCol(KnobToCol),
      cfg((FloatTy*) std::malloc(sizeof(FloatTy) * NUM_ROWS * NUM_COLS), std::free),
      result((FloatTy*) std::malloc(sizeof(FloatTy) * NUM_ROWS), std::free) {

        if (cfg == nullptr || result == nullptr)
          fatal_error("failed to allocate a config matrix of size " + std::to_string(NUM_ROWS * NUM_COLS));

        // we need to fill the cfg matrix with MISSING values, b/c some configs may not have all the knobs.
        FloatTy* cfg_raw = cfg.get();
        for (size_t i = 0; i < NUM_COLS; i++)
          cfg_raw[i] = MISSING_VAL;

      }

  // writes to the next row of the matrix, using the provided information
  void emplace_back(KnobSet const* Config, RandomQuantity const* IPC) {
    // locate and allocate the row, etc.
    FloatTy *row = cfgRow(nextFree);
    setResult(nextFree, IPC->mean());
    nextFree++;

    // fill the columns of the row in the cfg matrix
    for (auto const& Entry : *Config) {
      assert(KnobToCol.find(Entry.first) != KnobToCol.end() && "knob hasn't been assigned to a column.");
      FloatTy *cell = row + KnobToCol.at(Entry.first);

      Knob const* Knob = Entry.second.get();

      if (IntKnob const* IK = llvm::dyn_cast<IntKnob>(Knob)) {
        *cell = static_cast<FloatTy>(IK->getVal()); // NOTE: we're using the *non-scaled* value! this reduces the space of values.

      } else if (FlagKnob const* FK = llvm::dyn_cast<FlagKnob>(Knob)) {
        *cell = static_cast<FloatTy>(FK->getVal());

      } else if (OptLvlKnob const* OK = llvm::dyn_cast<OptLvlKnob>(Knob)) {
        *cell = static_cast<FloatTy>(OptLvlKnob::asInt(OK->getVal()));

      } else {
        fatal_error("non-exhaustive match failure in ConfigMatrix when flattening a config");
      }
    }
  }

  // number of rows written
  size_t size() const { return nextFree; }

private:

  FloatTy getResult (size_t row) const { return result.get()[row]; }
  void setResult(size_t row, FloatTy v) { result.get()[row] = v; }

  FloatTy* cfgRow(size_t row) {
    return cfg.get() + (row * NUM_COLS);
  }

  size_t nextFree;  // in terms of rows
  const size_t NUM_ROWS;
  const size_t NUM_COLS;
  std::unordered_map<std::string, size_t> const& KnobToCol;
  Array cfg;  // cfg[rowNum][i]  must be written as  cfg[(rowNum * ncol) + i]
  Array result;
};


KnobSet PseudoBayesTuner::generateConfig() {
  if (Versions.size() == 0)
    fatal_error("cannot generate a config with no prior!");

  /////////////////////
  // establish prior

  std::vector<std::pair<KnobSet const*, RandomQuantity const*>> allConfigs;
  std::unordered_map<std::string, size_t> KnobToCol;
  size_t knobsPerConfig = 0;
  { // we need to count how many configs and knobs we are working with, and determine a stable mapping
    // of knob names to column numbers.
    size_t freeCol = 0;
    for (auto const& Entry : Versions) {

      // a code version is only usable if it has some observations.
      auto const& RQ = Entry.second.getIPC();
      if (RQ.size() == 0)
        continue;

      auto const& Configs = Entry.second.getConfigs();
      if (Configs.size() == 0)
        fatal_error("A code version is missing a knob configuration!");

      // check for any new knobs we haven't seen in a prior config.
      for (auto const& Config : Configs) {
        allConfigs.push_back({&Config, &RQ});
        knobsPerConfig = std::max(knobsPerConfig, Config.size());

        for (auto const& Entry : Config)
          if (KnobToCol.find(Entry.first) == KnobToCol.end())
            KnobToCol[Entry.first] = freeCol++;

      }
    }
  }

  const size_t numConfigs = allConfigs.size();
  if (numConfigs == 0)
    fatal_error("no usable configurations. please collect IPC measurements.");

  //////
  // split the data into training and validation sets

  size_t validateRows = std::max(1, (int) std::round(HELDOUT_RATIO * numConfigs));
  size_t trainingRows = numConfigs - validateRows;
  assert(validateRows < allConfigs.size());

  ConfigMatrix train(trainingRows, knobsPerConfig, KnobToCol);
  ConfigMatrix validate(validateRows, knobsPerConfig, KnobToCol);

  // shuffle the data
  std::shuffle(allConfigs.begin(), allConfigs.end(), RNG);

  // partition
  auto I = allConfigs.cbegin();
  for (size_t cnt = 0; cnt < validateRows; I++, cnt++)
    validate.emplace_back(I->first, I->second);

  for (; I < allConfigs.cend(); I++)
    train.emplace_back(I->first, I->second);



  fatal_error("TODO: train an XGBoost model on this data!");
}

} // end namespace