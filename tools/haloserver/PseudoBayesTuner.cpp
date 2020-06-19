#include "halo/tuner/PseudoBayesTuner.h"
#include "halo/tuner/RandomTuner.h"
#include "halo/nlohmann/util.hpp"

#include <xgboost/c_api.h>

#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <set>

namespace halo {

PseudoBayesTuner::PseudoBayesTuner(nlohmann::json const& Config, KnobSet const& BaseKnobs,  std::unordered_map<std::string, CodeVersion> &Versions)
  : BaseKnobs(BaseKnobs), Versions(Versions),
    RNG(config::getServerSetting<uint64_t>("seed", Config)),
    TopTaken(config::getServerSetting<size_t>("pbtuner-top-taken", Config)),
    SearchSz(config::getServerSetting<size_t>("pbtuner-search-size", Config)),
    MIN_PRIOR(config::getServerSetting<size_t>("pbtuner-min-prior", Config)),
    HELDOUT_RATIO(config::getServerSetting<float>("pbtuner-heldout-ratio", Config)),
    ExploreRatio(config::getServerSetting<float>("pbtuner-explore-ratio", Config)),
    EnergyLvl(config::getServerSetting<float>("pbtuner-energy-level", Config)) {
      assert(0 < HELDOUT_RATIO && HELDOUT_RATIO < 1);
      assert(0 <= ExploreRatio && ExploreRatio <= 1);
      assert(0 <= EnergyLvl && EnergyLvl <= 100);
      assert(0 < TopTaken && TopTaken <= SearchSz);
      assert(4 <= MIN_PRIOR);
  }

////////////////////////////////////////////////////////////////////////////////////
// obtains a config, generating them if we've run out of cached ones
KnobSet PseudoBayesTuner::getConfig() {
  if (GeneratedConfigs.size() == 0) {
    auto Error = generateConfigs();
    if (Error) {
      // log it
      info(Error);
      llvm::consumeError(std::move(Error));

      // an error can happen if we have an insufficient prior.
      // so we return a random config instead to help establish one.
      return RandomTuner::randomFrom(BaseKnobs, RNG);
    }
  }

  assert(GeneratedConfigs.size() > 0);

  KnobSet KS = GeneratedConfigs.front();
  GeneratedConfigs.pop_front();
  return KS;
}
////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////
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
        for (size_t i = 0; i < NUM_ROWS * NUM_COLS; i++)
          cfg_raw[i] = MISSING_VAL;

        // for sanity, we also fill the results vector with zeroes
        FloatTy* result_raw = result.get();
        for (size_t i = 0; i < NUM_ROWS; i++)
          result_raw[i] = 0;

      }

  // writes to the next row of the matrix, using the provided information
  void emplace_back(KnobSet const* Config, RandomQuantity const* IPC) {
    setResult(nextFree, IPC->mean());
    emplace_back(Config);
  }

  // writes to the next row of the matrix, using the provided information
  void emplace_back(KnobSet const* Config) {
    // locate and allocate the row, etc.
    FloatTy *row = cfgRow(nextFree++);

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

  // total number of rows written to the cfg matrix (and correspondingly the results vector)
  // this does _not_ return the total row capacity
  size_t rows() const { return nextFree; }

  // total number of columns in the cfg matrix
  size_t cols() const { return NUM_COLS; }

  // total row capacity
  size_t max_rows() const {return NUM_ROWS; }

  // returns a pointer to the CFG matrix
  FloatTy const* getCFG() const { return cfg.get(); }

  // return a pointer to the results / IPC vector
  FloatTy const* getResults() const { return result.get(); }

  // get a specific result from the results array
  FloatTy getResult(size_t row) const { return result.get()[row]; }

private:

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
/////// end class ConfigMatrix



////////////////////////////////////////////////////////////////////////////////////
// Creates an initial XGB Booster
void initBooster(const DMatrixHandle dmats[],
                      bst_ulong len,
                      BoosterHandle *out) {

  XGBoosterCreate(dmats, len, out);
  // NOTE: all of these settings were picked arbitrarily
#ifdef NDEBUG
  XGBoosterSetParam(*out, "silent", "1");
#endif
  XGBoosterSetParam(*out, "booster", "gbtree");
  XGBoosterSetParam(*out, "objective", "reg:linear");
  XGBoosterSetParam(*out, "max_depth", "5");
  XGBoosterSetParam(*out, "eta", "0.3");
  XGBoosterSetParam(*out, "min_child_weight", "1");
  XGBoosterSetParam(*out, "subsample", "0.5");
  XGBoosterSetParam(*out, "colsample_bytree", "1");
  XGBoosterSetParam(*out, "num_parallel_tree", "4");
}
////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////
// the training step. returns the best model learned (in a serialized form)
std::vector<char> runTraining(ConfigMatrix const& trainData, ConfigMatrix const& validateData,
                  unsigned MaxLearnIters=500, unsigned MaxLearnPastBest=0) {

  // must be an array. not sure why.
  // this thing manages the XGBoost view of the config matrix.
  DMatrixHandle train[1];
  {
    // add config data
    if (XGDMatrixCreateFromMat(trainData.getCFG(),
                                trainData.rows(), trainData.cols(),
                                ConfigMatrix::MISSING_VAL, &train[0]))
      fatal_error("XGDMatrixCreateFromMat failed.");

    // add labels, aka, IPCs corresponding to each configuration
    if (XGDMatrixSetFloatInfo(train[0], "label", trainData.getResults(), trainData.rows()))
      fatal_error("XGDMatrixSetFloatInfo failed.");
  }

  // make a boosted model so we can start training
  BoosterHandle booster;
  initBooster(train, 1, &booster);


  // setup validation dataset
  const size_t validateRows = validateData.rows();
  DMatrixHandle h_validate;
  XGDMatrixCreateFromMat(validateData.getCFG(),
                         validateData.rows(), validateData.cols(),
                         ConfigMatrix::MISSING_VAL, &h_validate);

  double bestErr = std::numeric_limits<double>::max();
  std::vector<char> bestModel;
  unsigned bestIter;
  for (unsigned i = 0; i < MaxLearnIters; i++) {
    int rv = XGBoosterUpdateOneIter(booster, i, train[0]);
    assert(rv == 0 && "learn step failed");


    // evaluate this new model
    bst_ulong out_len;
    const float *predict;
    rv = XGBoosterPredict(booster, h_validate, 0, 0, &out_len, &predict);

    assert(rv == 0 && "predict failed");
    assert(out_len == validateRows && "doesn't make sense!");

    // calculate Root Mean Squared Error (RMSE) of predicting our held-out set
    double err = 0.0;
    for (size_t i = 0; i < validateRows; i++) {
      double actual = validateData.getResult(i);
      double guess = predict[i];

      clogs() << "actual = " << actual
                << ", guess = " << guess << "\n";

      err += std::pow(actual - guess, 2) / validateRows;
    }
    err = std::sqrt(err);

    clogs() << "RMSE = " << err << "\n\n";

    // is this model better than we've seen before?
    if (err <= bestErr || bestModel.size() == 0) {
      const char* ro_view;
      bst_ulong ro_len;
      rv = XGBoosterGetModelRaw(booster, &ro_len, &ro_view);
      assert(rv == 0 && "can't view model");

      // save this new best model
      bestErr = err;
      bestIter = i;

      // drop the old model
      bestModel.clear();

      // copy the read-only view of the model to our vector
      for (size_t i = 0; i < ro_len; i++)
        bestModel.push_back(ro_view[i]);

    } else if (i - bestIter < MaxLearnPastBest) {
          // we're willing to go beyond the minima so-far.
          clogs() << "going beyond best-seen";
    }  else {
      // stop training, since it's not any getting better
      break;
    }
  }

  assert(bestModel.size() != 0 && "training failed?");

  clogs() << "Learned model with error: " << bestErr << "\n";

  // clean-up!
  XGBoosterFree(booster);
  XGDMatrixFree(train[0]);
  XGDMatrixFree(h_validate);

  return bestModel;
}
////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////
// Leveraging a model of the performance for configurations, we search the configuration-space
// for new high-value configurations based on our prior experience.
void PseudoBayesTuner::surrogateSearch(std::vector<char> const& SerializedModel,
                     KnobSet const* bestConfig,
                     size_t knobsPerConfig,
                     std::unordered_map<std::string, size_t> const& KnobToCol) {

  // search by first generating a bunch of configurations
  size_t ExploreSz = std::round(ExploreRatio * SearchSz);
  size_t ExploitSz = SearchSz - ExploreSz;

  ConfigMatrix searchData(SearchSz, knobsPerConfig, KnobToCol);
  std::vector<KnobSet> searchConfig;

  { // EXPLORE
    for (size_t i = 0; i < ExploreSz; i++) {
      searchConfig.push_back(RandomTuner::randomFrom(BaseKnobs, RNG));
      searchData.emplace_back(&(searchConfig.back()));
    }
  }

  { // EXPLOIT
    KnobSet Best{*bestConfig};
    Best.copyingUnion(BaseKnobs); // we want to expand this best config with all possible tuning knobs.
    for (size_t i = 0; i < ExploitSz; i++) {
      searchConfig.push_back(RandomTuner::nearby(Best, RNG, EnergyLvl));
      searchData.emplace_back(&(searchConfig.back()));
    }
  }


  // load the model
  BoosterHandle model;
  XGBoosterCreate(NULL, 0, &model);
  int rv = XGBoosterLoadModelFromBuffer(model, SerializedModel.data(), SerializedModel.size());
  assert(rv == 0);

  // now, use the model to predict the quality of the configurations we generated.
  DMatrixHandle h_test;
  XGDMatrixCreateFromMat(searchData.getCFG(), searchData.rows(), searchData.cols(), ConfigMatrix::MISSING_VAL, &h_test);
  bst_ulong out_len;
  const float *out;
  XGBoosterPredict(model, h_test, 0, 0, &out_len, &out);


  // take the top N best predictions
  using SetKey = std::pair<uint32_t, float>;

  struct lessThan {
    constexpr bool operator()(const SetKey &lhs, const SetKey &rhs) const
    {
        return lhs.second < rhs.second;
    }
  };

  // the IPCs contained in this set are the negative of the predicted IPC.
  // this is done so that the multiset works for us in keeping the smallest N
  // elements, where the smallest negative IPC = largest IPC
  std::multiset<SetKey, lessThan> Best;
  for (uint32_t i = 0; i < out_len; i++) {
    float predictedIPC = out[i];

    clogs() << "prediction[" << i << "]=" << predictedIPC << "\n";

    auto Cur = std::make_pair(i, -predictedIPC);

    if (Best.size() < TopTaken) {
      Best.insert(Cur);
      continue;
    }

    auto UB = Best.upper_bound(Cur);
    if (UB != Best.end()) {
      // then UB is greater than Cur
      Best.erase(UB);
      Best.insert(Cur);
    }
  }

  // now we've got the top N configurations.
  for (auto Entry : Best) {
    auto Chosen = Entry.first;
    clogs() << "chose config " << Chosen
            << " with estimated IPC " << -Entry.second
            << "\n";
    GeneratedConfigs.push_back(searchConfig[Chosen]);
  }

  // cleanup
  XGBoosterFree(model);
  XGDMatrixFree(h_test);

} // end surrogateSearch
////////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////////
// This is the juicy part that kicks-off all the work!
//
llvm::Error PseudoBayesTuner::generateConfigs() {
  if (Versions.size() == 0)
    return makeError("cannot generate a config with no prior!");

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
        return makeError("A code version is missing a knob configuration!");

      // check for any new knobs we haven't seen in a prior config.
      for (auto const& Config : Configs) {
        allConfigs.push_back({&Config, &RQ});
        knobsPerConfig = std::max(knobsPerConfig, Config.size());

        for (auto const& Entry : Config)
          if (KnobToCol.find(Entry.first) == KnobToCol.end())
            KnobToCol[Entry.first] = freeCol++;

      }
    }
  } // end block

  const size_t numConfigs = allConfigs.size();
  if (numConfigs < MIN_PRIOR)
    return makeError("insufficient usable configurations. please collect more IPC measurements with varying configs.");


  // Find the best config.
  KnobSet const* bestConfig = nullptr;
  {
    float BestIPC = std::numeric_limits<float>::min();

    for (auto const& Entry : allConfigs) {
      float IPC = Entry.second->mean();
      if (IPC > BestIPC) {
        bestConfig = Entry.first;
        BestIPC = IPC;
      }
    }

    assert(bestConfig != nullptr);
  }

  //////
  // split the data into training and validation sets

  size_t validateRows = std::max(2, (int) std::round(HELDOUT_RATIO * numConfigs));
  size_t trainingRows = numConfigs - validateRows;

  // some sanity checks
  assert(validateRows < allConfigs.size());
  assert(validateRows > 0);
  assert(trainingRows > 0);

  std::vector<char> surrogateModel;

  ///////////
  // train an XGBoost model on the trainData dataset w.r.t the validation dataset.
  //
  // we want a model which has learned whether a configuration will yield a good IPC or not.
  {
    ConfigMatrix trainData(trainingRows, knobsPerConfig, KnobToCol);
    ConfigMatrix validateData(validateRows, knobsPerConfig, KnobToCol);

    // shuffle the data so validation and training sets are allocated at random
    std::shuffle(allConfigs.begin(), allConfigs.end(), RNG);

    // partition
    auto I = allConfigs.cbegin();
    for (size_t cnt = 0; cnt < validateRows; I++, cnt++)
      validateData.emplace_back(I->first, I->second);

    for (; I < allConfigs.cend(); I++)
      trainData.emplace_back(I->first, I->second);

    // at this point, we're done with the allConfigs.
    allConfigs.clear();

    surrogateModel = runTraining(trainData, validateData);
  }

  ///////////
  // finally, we use this model to search the configuration space, saving
  // the good configurations we've found.

  surrogateSearch(surrogateModel, bestConfig, knobsPerConfig, KnobToCol);

  return llvm::Error::success();
} // end of generateConfigs

} // end namespace