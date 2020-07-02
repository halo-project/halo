#include "halo/tuner/PseudoBayesTuner.h"
#include "halo/tuner/RandomTuner.h"
#include "halo/nlohmann/util.hpp"

#include <xgboost/c_api.h>

#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <set>

// checks the return value of calls to XGBoost C API
#define safe_xgboost(call) {                                                \
int err = (call);                                                           \
if (err != 0) {                                                             \
  halo::fatal_error(std::string(__FILE__) + ":" + std::to_string(__LINE__)  \
                      + " during " + #call + ": " + XGBGetLastError());     \
}                                                                           \
}


namespace halo {

PseudoBayesTuner::PseudoBayesTuner(nlohmann::json const& Config, KnobSet const& BaseKnobs,  std::unordered_map<std::string, CodeVersion> &Versions)
  : BaseKnobs(BaseKnobs), Versions(Versions),
    RNG(config::getServerSetting<uint64_t>("seed", Config)),
    LearnIters(config::getServerSetting<size_t>("pbtuner-learn-iters", Config)),
    TotalBatchSz(config::getServerSetting<size_t>("pbtuner-batch-size", Config)),
    SearchSz(config::getServerSetting<size_t>("pbtuner-surrogate-batch-size", Config)),
    MIN_PRIOR(config::getServerSetting<size_t>("pbtuner-min-prior", Config)),
    HELDOUT_RATIO(config::getServerSetting<float>("pbtuner-heldout-ratio", Config)),
    ExploreRatio(config::getServerSetting<float>("pbtuner-surrogate-explore-ratio", Config)),
    EnergyLvl(config::getServerSetting<float>("pbtuner-energy-level", Config)) {
      assert(0 < HELDOUT_RATIO && HELDOUT_RATIO < 1);
      assert(0 <= ExploreRatio && ExploreRatio <= 1);
      assert(0 <= EnergyLvl && EnergyLvl <= 100);
      assert(4 <= MIN_PRIOR);

      const float MainBatchExploreRatio = config::getServerSetting<float>("pbtuner-explore-ratio", Config);
      assert(0 <= MainBatchExploreRatio && MainBatchExploreRatio <= 1.0f);

      ExploitBatchSz = TotalBatchSz - std::floor(MainBatchExploreRatio * TotalBatchSz);
  }

////////////////////////////////////////////////////////////////////////////////////
// obtains a config, generating them if we've run out of cached ones
KnobSet PseudoBayesTuner::getConfig(std::string CurrentLib) {
  if (Manager.sizeTop() == 0) {
    auto Error = generateConfigs(CurrentLib);

    if (Error) {
      // log it. an error can happen if we have an insufficient prior.
      info(Error);

      // if we have insufficient prior, give a random one
      if (Manager.size() < MIN_PRIOR) {
        llvm::consumeError(std::move(Error));
        return Manager.genRandom(BaseKnobs, RNG);
      }

      fatal_error("some other issue occurred");
    }

    // we always want to make sure we're not overfitting to what we already know,
    // so we fill the remaining up to the batch size with random ones.
    while (Manager.sizeTop() < TotalBatchSz)
      Manager.addTop(Manager.genRandom(BaseKnobs, RNG));
  }

  assert(Manager.sizeTop() > 0);
  return Manager.popTop();
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
    if (nextFree == NUM_ROWS)
      fatal_error("ConfigMatrix is full!");

    setResult(nextFree, IPC->mean());
    emplace_back(Config);
  }

  // writes to the next row of the matrix, using the provided information
  void emplace_back(KnobSet const* Config) {
    if (nextFree == NUM_ROWS)
      fatal_error("ConfigMatrix is full!");

    // locate and allocate the row, etc.
    FloatTy *row = cfgRow(nextFree++);

    // fill the columns of the row in the cfg matrix
    for (auto const& Entry : *Config) {
      assert(KnobToCol.find(Entry.first) != KnobToCol.end() && "knob hasn't been assigned to a column.");
      FloatTy *cell = row + KnobToCol.at(Entry.first);

      Knob const* Knob = Entry.second.get();

      if (IntKnob const* IK = llvm::dyn_cast<IntKnob>(Knob)) {
        IK->applyVal([&](int Val) {
          *cell = static_cast<FloatTy>(Val); // NOTE: we're using the *non-scaled* value! this reduces the space of values.
        });

      } else if (FlagKnob const* FK = llvm::dyn_cast<FlagKnob>(Knob)) {
        FK->applyVal([&](int Val) {
          *cell = static_cast<FloatTy>(Val);
        });

      } else if (OptLvlKnob const* OK = llvm::dyn_cast<OptLvlKnob>(Knob)) {
        OK->applyVal([&](OptLvlKnob::LevelTy Val) {
          *cell = static_cast<FloatTy>(OptLvlKnob::asInt(Val));
        });

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

  safe_xgboost(XGBoosterCreate(dmats, len, out));

  // NOTE: all of the char*'s passed to XGB here need to have a lifetime
  // that exceeds the lifetime of this function call. It basically has
  // to be a string literal that is passed in.

#ifdef NDEBUG
  // 0 = silent, 1 = warning, 2 = info, 3 = debug. default is 1
  safe_xgboost(XGBoosterSetParam(*out, "verbosity", "0"));
#endif

  // TODO: (1) early stopping for training
  //
  //       (2) start with appropriate constants. set "base_score" to be the mean of all instances
  //           this one is less important than early stopping. it helps you see if you're actually learning
  //           or predicting a constant.

  // FIXME: all of these settings were picked arbitrarily and/or with specific machines in mind!
  // Read here for info: https://xgboost.readthedocs.io/en/latest/parameter.html
  safe_xgboost(XGBoosterSetParam(*out, "booster", "gbtree"));
  safe_xgboost(XGBoosterSetParam(*out, "nthread", "2")); // TODO: maybe just use 1 thread for speed lol.
  safe_xgboost(XGBoosterSetParam(*out, "objective", "reg:squarederror"));
  safe_xgboost(XGBoosterSetParam(*out, "max_depth", "3"));  // somewhere between 2 and 5 for our data set size

  // Step size shrinkage used in update to prevents overfitting. Default = 0.3
  safe_xgboost(XGBoosterSetParam(*out, "eta", "0.3"));

  // most important parameter, how minimum number of datapoints in a leaf.
  //
  // from docs:
  //
  // Minimum sum of instance weight (hessian) needed in a child.
  // If the tree partition step results in a leaf node with the sum
  // of instance weight less than min_child_weight, then the building
  // process will give up further partitioning. In linear regression task,
  // this simply corresponds to minimum number of instances needed to be in each node.
  // The larger min_child_weight is, the more conservative the algorithm will be.
  //
  // brian suggests at least 2.
  safe_xgboost(XGBoosterSetParam(*out, "min_child_weight", "2"));

  safe_xgboost(XGBoosterSetParam(*out, "subsample", "0.75"));
  safe_xgboost(XGBoosterSetParam(*out, "colsample_bytree", "1"));
  safe_xgboost(XGBoosterSetParam(*out, "num_parallel_tree", "4"));

}
////////////////////////////////////////////////////////////////////////////////////


void initializeDMatrixHandle(DMatrixHandle *handle, ConfigMatrix const& Data) {
  // add config data
  safe_xgboost(XGDMatrixCreateFromMat(Data.getCFG(),
                              Data.rows(), Data.cols(),
                              ConfigMatrix::MISSING_VAL, handle));

  // add labels, aka, IPCs corresponding to each configuration
  safe_xgboost(XGDMatrixSetFloatInfo(*handle, "label", Data.getResults(), Data.rows()));
}


////////////////////////////////////////////////////////////////////////////////////
// the training step. returns the best model learned (in a serialized form)
// See here for reference: https://github.com/dmlc/xgboost/blob/master/demo/c-api/c-api-demo.c
// since the C API is quite poorly documented.
std::vector<char> runTraining(ConfigMatrix const& trainData, ConfigMatrix const& validateData,
                  unsigned LearnIters) {

  // load the data
  DMatrixHandle dtrain, dtest;
  initializeDMatrixHandle(&dtrain, trainData);
  initializeDMatrixHandle(&dtest, validateData);

  // create the booster
  const unsigned NUM_DMATS = 2;
  BoosterHandle booster;
  DMatrixHandle eval_dmats[NUM_DMATS] = {dtrain, dtest};
  initBooster(eval_dmats, NUM_DMATS, &booster);

  // train and evaluate for 10 iterations
  int n_trees = LearnIters;
  const char* eval_names[NUM_DMATS] = {"train", "test"};
  const char* eval_result = nullptr;
  for (int i = 0; i < n_trees; ++i) {
    safe_xgboost(XGBoosterUpdateOneIter(booster, i, dtrain));
    safe_xgboost(XGBoosterEvalOneIter(booster, i, eval_dmats, eval_names, NUM_DMATS, &eval_result));
    info(eval_result);
  }

  // save the model. we copy the read-only view of the model to a vector
  std::vector<char> bestModel;
  const char* ro_view;
  bst_ulong ro_len;
  safe_xgboost(XGBoosterGetModelRaw(booster, &ro_len, &ro_view));
  for (size_t i = 0; i < ro_len; i++)
    bestModel.push_back(ro_view[i]);


  // clean-up!
  safe_xgboost(XGBoosterFree(booster));
  safe_xgboost(XGDMatrixFree(dtrain));
  safe_xgboost(XGDMatrixFree(dtest));

  return bestModel;
}
////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////
// Leveraging a model of the performance for configurations, we search the configuration-space
// for new high-value configurations based on our prior experience.
llvm::Error PseudoBayesTuner::surrogateSearch(std::vector<char> const& SerializedModel,
                     CodeVersion const& bestVersion,
                     size_t knobsPerConfig,
                     std::unordered_map<std::string, size_t> const& KnobToCol) {

  // search by first generating a bunch of configurations
  size_t ExploreSz = std::round(ExploreRatio * SearchSz);
  size_t ExploitSz = SearchSz - ExploreSz;
  size_t UpdateSz = SearchSz / 8;

  ConfigMatrix searchMatrix(SearchSz + UpdateSz, knobsPerConfig, KnobToCol);
  std::vector<KnobSet> searchConfig;


  { // GET FRESH PREDICTIONS FOR PREVIOUSLY GENERATED CONFIGS
    for (size_t i = 0; i < UpdateSz; i++) {
      searchConfig.push_back(Manager.genPrevious(RNG));
      searchMatrix.emplace_back(&searchConfig.back());
    }
  }

  { // EXPLORE
    for (size_t i = 0; i < ExploreSz; i++) {
      searchConfig.push_back(Manager.genRandom(BaseKnobs, RNG));
      searchMatrix.emplace_back(&(searchConfig.back()));
    }
  }

  { // EXPLOIT
    std::vector<KnobSet> const& SimilarConfigs = bestVersion.getConfigs();
    std::uniform_int_distribution<size_t> Chooser(0, SimilarConfigs.size()-1);
    for (size_t i = 0; i < ExploitSz; i++) {
      KnobSet GoodConfig = SimilarConfigs[Chooser(RNG)];
      GoodConfig.copyingUnion(BaseKnobs); // we want to expand this good config with all possible tuning knobs.

      searchConfig.push_back(Manager.genNearby(GoodConfig, RNG, EnergyLvl));
      searchMatrix.emplace_back(&(searchConfig.back()));
    }
  }

  // load the model
  BoosterHandle model;
  safe_xgboost(XGBoosterCreate(NULL, 0, &model));
  safe_xgboost(XGBoosterLoadModelFromBuffer(model, SerializedModel.data(), SerializedModel.size()));

  // now, use the model to predict the quality of the configurations we generated.
  DMatrixHandle h_test;
  safe_xgboost(XGDMatrixCreateFromMat(searchMatrix.getCFG(), searchMatrix.rows(), searchMatrix.cols(), ConfigMatrix::MISSING_VAL, &h_test));
  bst_ulong out_len;
  const float *out;
  safe_xgboost(XGBoosterPredict(model, h_test, 0, 0, &out_len, &out)); // predict!!
  assert(out_len == searchMatrix.rows());


  // take the top N best predictions
  using SetKey = std::pair<uint32_t, float>;

  struct lessThan {
    constexpr bool operator()(const SetKey &lhs, const SetKey &rhs) const
    {
        return lhs.second < rhs.second;
    }
  };

  // TODO: what if we pruned the top N predictions so that only those
  // which are higher than the mean predicted IPC of the best version
  // appear in that list. It could be the case that the model doesn't think
  // _anything_ is better than what we've got, and we should return an error
  // in that case so the tuner generates a random one instead.

  // the IPCs contained in this multiset are the negative of the predicted IPC.
  // this is done so that the multiset works for us in keeping the smallest N
  // elements, where the smallest negative IPC = largest IPC
  std::multiset<SetKey, lessThan> Best;

  for (size_t i = 0; i < searchMatrix.rows(); i++) {
    float predictedIPC = out[i];

    Manager.setPredictedQuality(searchConfig.at(i), predictedIPC);

    // clogs() << "prediction[" << i << "]=" << predictedIPC << "\n";

    // we don't want to return previous configurations, which are
    // put at the front of the matrix.
    // we just added them to the matrix to update their predicted quality.
    // to help the StatisticalStopper.
    if (i < UpdateSz)
      continue;

    auto Cur = std::make_pair(i, -predictedIPC);

    if (Best.size() < ExploitBatchSz) {
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

  // now we've got the top N-ish configurations.
  for (auto Entry : Best) {
    auto Chosen = Entry.first;
    clogs() << "chose config " << Chosen
            << " with estimated IPC " << -Entry.second
            << "\n";
    Manager.addTop(searchConfig[Chosen]);
  }

  // cleanup
  safe_xgboost(XGBoosterFree(model));
  safe_xgboost(XGDMatrixFree(h_test));

  if (Manager.sizeTop() == 0)
    return makeError("pbtuner failed to find any good configurations");

  return llvm::Error::success();
} // end surrogateSearch
////////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////////
// This is the juicy part that kicks-off all the work!
//
llvm::Error PseudoBayesTuner::generateConfigs(std::string CurrentLib) {
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

  CodeVersion const& bestVersion = Versions.at(CurrentLib);

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

    surrogateModel = runTraining(trainData, validateData, LearnIters);
  }

  ///////////
  // finally, we use this model to search the configuration space, saving
  // the good configurations we've found.

  return surrogateSearch(surrogateModel, bestVersion, knobsPerConfig, KnobToCol);
} // end of generateConfigs

} // end namespace