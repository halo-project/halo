#include "halo/tuner/PseudoBayesTuner.h"
#include "halo/tuner/RandomTuner.h"
#include "halo/nlohmann/util.hpp"

#include <xgboost/c_api.h>
#include <gsl/gsl_rstat.h>

#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <set>
#include <regex>


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

      InitializeBoosterParams(Options);
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
    : nextFree(0), NUM_ROWS(numRows), NUM_COLS(numCols), FeatureMap(KnobToCol),
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
      assert(FeatureMap.find(Entry.first) != FeatureMap.end() && "knob hasn't been assigned to a column.");
      FloatTy *cell = row + FeatureMap.at(Entry.first);

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

  auto const& getFeatureMap() const { return FeatureMap; }

private:

  void setResult(size_t row, FloatTy v) { result.get()[row] = v; }

  FloatTy* cfgRow(size_t row) {
    return cfg.get() + (row * NUM_COLS);
  }

  size_t nextFree;  // in terms of rows
  const size_t NUM_ROWS;
  const size_t NUM_COLS;
  std::unordered_map<std::string, size_t> const& FeatureMap;
  Array cfg;  // cfg[rowNum][i]  must be written as  cfg[(rowNum * ncol) + i]
  Array result;
};
/////// end class ConfigMatrix



////////////////////////////////////////////////////////////////////////////////////
// Creates an initial XGB Booster

void PseudoBayesTuner::InitializeBoosterParams(BoosterParams& Options) {
#ifdef NDEBUG
  // 0 = silent, 1 = warning, 2 = info, 3 = debug. default is 1
  Options.insert({"verbosity", "0"});
#endif

  // Read here for info: https://xgboost.readthedocs.io/en/latest/parameter.html

  Options.insert({"booster", "gbtree"});
  Options.insert({"nthread", "2"}); // TODO: maybe just use 1 thread for speed l}l.
  Options.insert({"objective", "reg:squarederror"});
  Options.insert({"max_depth", "3"});  // somewhere between 2 and 5 for our data set s}ze

  // Step size shrinkage used in update to prevents overfitting. Default = 0.3
  Options.insert({"eta", "0.3"});

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
  Options.insert({"min_child_weight", "2"});

  Options.insert({"subsample", "0.75"});
  Options.insert({"colsample_bytree", "1"});
  Options.insert({"num_parallel_tree", "4"});
}


// NOTE: all of the strings in the Options must have a lifetime
// that exceeds the lifetime of the BoosterHandle!
void initBooster(const DMatrixHandle dmats[],
                      bst_ulong len,
                      BoosterHandle *out,
                      PseudoBayesTuner::BoosterParams const& Options) {

  safe_xgboost(XGBoosterCreate(dmats, len, out));

  clogs() << "BoosterParams:\n";
  for (auto const& Entry : Options) {
    clogs() << Entry.first << " = " << Entry.second << "\n";
    safe_xgboost(XGBoosterSetParam(*out, Entry.first.c_str(), Entry.second.c_str()));
  }
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

// In the Python bindings, various measures of feature importance is obtained through get_score:
//
//    https://xgboost.readthedocs.io/en/latest/python/python_api.html#xgboost.Booster.get_score
//
// The way it's implemented is through parsing the string dump of the model:
//
//    https://github.com/dmlc/xgboost/blob/02884b08aa3d3000baf4db47fc6c5cef90820a5d/python-package/xgboost/core.py#L1608
//
//  ... ugh.
void analyzeFeatureImportance(BoosterHandle booster, std::unordered_map<std::string, size_t> const& FeatureMap) {
  // first, we need to sort the features by column number (increasing order)
  using Pair = std::pair<llvm::StringRef, size_t>;
  std::vector<Pair> OrderedMap;
  for (auto const& Entry : FeatureMap)
    OrderedMap.push_back({Entry.first.c_str(), Entry.second});

  std::sort(OrderedMap.begin(), OrderedMap.end(),
    [](Pair const& A, Pair const& B) {
      return A.second < B.second;
  });

  // type names from here: https://github.com/dmlc/xgboost/blob/1d22a9be1cdeb53dfa9322c92541bc50e82f3c43/include/xgboost/feature_map.h#L79
  std::vector<const char*> FeatureTypes;
  std::vector<const char*> FeatureNames;

  for (auto const& Entry : OrderedMap) {
    logs() << Entry.second << " --> " << Entry.first << "\n";
    FeatureNames.push_back(Entry.first.data());
    FeatureTypes.push_back("float");
  }

  bst_ulong out_len;
  const char **out_models;
  safe_xgboost(XGBoosterDumpModelWithFeatures(booster,
      FeatureNames.size(), FeatureNames.data(), FeatureTypes.data(), /*with_stats=*/ 1, &out_len, &out_models));

  for (unsigned i = 0; i < out_len; ++i)
    clogs() << out_models[i];

  // TODO: Parse the dump to calculate importance metric that we care to know about.
}


////////////////////////////////////////////////////////////////////////////////////
// the training step. returns the best model learned (in a serialized form)
// See here for reference: https://github.com/dmlc/xgboost/blob/master/demo/c-api/c-api-demo.c
// since the C API is quite poorly documented.
std::vector<char> runTraining(ConfigMatrix const& trainData, ConfigMatrix const& validateData,
                  PseudoBayesTuner::BoosterParams const& Options, unsigned LearnIters) {

  // load the data
  DMatrixHandle dtrain, dtest;
  initializeDMatrixHandle(&dtrain, trainData);
  initializeDMatrixHandle(&dtest, validateData);

  // create the booster
  const unsigned NUM_DMATS = 2;
  BoosterHandle booster;
  DMatrixHandle eval_dmats[NUM_DMATS] = {dtrain, dtest};
  initBooster(eval_dmats, NUM_DMATS, &booster, Options);

  // We have to manually implement early-stopping here because in XGB, the
  // early-stopping training loop is implemented in Python:
  // https://github.com/dmlc/xgboost/blob/eb067c1c34d03950f6c7e195b852fc709e313df3/python-package/xgboost/callback.py#L149
  //
  // ours differs in that we stop the first time the error is not decreasing.

  std::vector<char> bestModel;
  float bestErr = std::numeric_limits<float>::max();

  // yes, sadly, we have to parse the error out of the string! The XGBoost C API is barren and stringy.
  // based on example from  https://en.cppreference.com/w/cpp/regex/regex_match
  const std::regex TestErrorRegex(".*test-.*:(.+)$");
  std::smatch pieces_match;

  const int MAX_ITERS = LearnIters;
  const char* eval_names[NUM_DMATS] = {"train", "test"};
  const char* eval_result = nullptr;
  for (int i = 0; i < MAX_ITERS; ++i) {
    // learn and evaluate
    safe_xgboost(XGBoosterUpdateOneIter(booster, i, dtrain));
    safe_xgboost(XGBoosterEvalOneIter(booster, i, eval_dmats, eval_names, NUM_DMATS, &eval_result));

    info(eval_result);
    std::string ResultStr(eval_result);

    if (!std::regex_match(ResultStr, pieces_match, TestErrorRegex))
      fatal_error("failed to parse test error from eval_result");

    std::ssub_match sub_match = pieces_match[1];
    std::string piece = sub_match.str();

    // clogs() << "parsed test error = \"" << piece << "\"\n";
    float testErr = std::stof(piece);

    // now compare the error with best so far
    if (testErr <= bestErr) {
      // this one is better
      bestErr = testErr;

      // save the model. we copy the read-only view of the model to a vector
      bestModel.clear();
      const char* ro_view;
      bst_ulong ro_len;
      safe_xgboost(XGBoosterGetModelRaw(booster, &ro_len, &ro_view));
      for (size_t i = 0; i < ro_len; i++)
        bestModel.push_back(ro_view[i]);

    } else {
      info("Early stop!");
      break;
    }
  }

  assert(bestModel.size() > 0);

  analyzeFeatureImportance(booster, trainData.getFeatureMap());

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

    // partition and compute the base score, i.e., the initial prediction score of all instances, global bias

    auto I = allConfigs.cbegin();
    for (size_t cnt = 0; cnt < validateRows; I++, cnt++)
      validateData.emplace_back(I->first, I->second);

    for (; I < allConfigs.cend(); I++)
      trainData.emplace_back(I->first, I->second);

    // at this point, we're done with the allConfigs.
    allConfigs.clear();

    {
      gsl_rstat_workspace *stats = gsl_rstat_alloc();
      // Compute the Booster's base_score. we do it separately in two loops here because
      // we want to avoid calling RandomQuantity::mean() twice because
      // currently it recomputes the mean on every call.
      //
      // The base_score is the initial prediction score of all instances, i.e., the global bias.
      // As Brian suggested, we set it to be the mean of all the data we have.

      for (size_t i = 0; i < validateData.rows(); i++)
        gsl_rstat_add(validateData.getResult(i), stats);

      for (size_t i = 0; i < trainData.rows(); i++)
        gsl_rstat_add(trainData.getResult(i), stats);

      Options.insert({"base_score", std::to_string(gsl_rstat_mean(stats))});
      gsl_rstat_free(stats);
    }

    surrogateModel = runTraining(trainData, validateData, Options, LearnIters);
  }

  ///////////
  // finally, we use this model to search the configuration space, saving
  // the good configurations we've found.

  return surrogateSearch(surrogateModel, bestVersion, knobsPerConfig, KnobToCol);
} // end of generateConfigs

} // end namespace