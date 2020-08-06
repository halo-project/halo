#include "halo/tuner/ConfigManager.h"
#include "halo/tuner/RandomTuner.h"
#include "Logging.h"
#include <utility>

namespace halo {

void ConfigManager::insert(KnobSet const& KS) {
  Database.insert(std::make_pair<KnobSet, ConfigManager::Metadata>(KnobSet(KS), {}));
}

KnobSet ConfigManager::retryLoop(KnobSet const& Initial,
                  std::function<KnobSet(KnobSet&&)> &&Generator,
                  unsigned Limit) {

  KnobSet KS(Initial);
  for (unsigned Tries = 0; Tries < Limit; ++Tries) {
    KS = Generator(std::move(KS));
    if (Database.count(KS) == 0) // unique?
      break;
  }

  if (Database.count(KS) != 0)  // not unique?
    return KS; // just return this knob set that is already in the DB.

  insert(KS);
  return KS;
}

KnobSet ConfigManager::genRandom(KnobSet const& BaseKnobs, std::mt19937_64 &RNG) {
  return retryLoop(BaseKnobs,
    [&](KnobSet &&KS) {
      return RandomTuner::randomFrom(std::move(KS), RNG);
    }
  );
}

KnobSet ConfigManager::genNearby(KnobSet const& GoodConfig, std::mt19937_64 &RNG, float EnergyLvl) {
  return retryLoop(GoodConfig,
    [&](KnobSet &&KS) {
      return RandomTuner::nearby(std::move(KS), RNG, EnergyLvl);
    }
  );
}

KnobSet ConfigManager::genPrevious(std::mt19937_64 &RNG, bool ExcludeTop) {
  size_t Sz = Database.size();
  if (Sz == 0)
    fatal_error("no previous config to return!");

  std::uniform_int_distribution<size_t> Gen(0, Sz-1);

  unsigned MaxTries = 3; // must be > 0
  auto I = Database.begin();
  do {
    size_t Chosen = Gen(RNG);
    I = Database.begin();

    // sadly O(Sz)
    for (; I != Database.end(); ++I, --Chosen)
      if (Chosen == 0) {
        if (!ExcludeTop || !I->second.BeenInTop)
          return I->first;
        else
          break;
      }

    MaxTries--;
  } while (MaxTries > 0);

  warning("lookup failure in genPrevious. returning an arbitrary previous config");
  return I->first;
}

llvm::Optional<KnobSet> ConfigManager::genExpertOpinion(KnobSet const& BaseKnobs) {
  KnobSet KS(BaseKnobs);
  KS.unsetAll();

  switch(Opines) {

    case 1: {
      // set the lesser-used but seemingly safe & good flags
      KS.lookup<FlagKnob>(named_knob::IPRA).setFlag(true);
      KS.lookup<FlagKnob>(named_knob::PBQP).setFlag(true);
      KS.lookup<FlagKnob>(named_knob::AttributorEnable).setFlag(true);
      KS.lookup<FlagKnob>(named_knob::ExperimentalAlias).setFlag(true);
    }; // FALL-THROUGH

    case 0: {
      // equivalent to -O3 -mcpu=native
      KS.lookup<OptLvlKnob>(named_knob::OptimizeLevel)
        .setVal(llvm::PassBuilder::OptimizationLevel::O3);

      KS.lookup<OptLvlKnob>(named_knob::CodegenLevel)
        .setVal(llvm::PassBuilder::OptimizationLevel::O3);

      KS.lookup<FlagKnob>(named_knob::NativeCPU).setFlag(true);
    } break;


    default:
      return llvm::None; // run out of ideas
  };

  Opines++;
  insert(KS);
  return KS;
}

} // namespace halo