#include "halo/tuner/ConfigManager.h"
#include "halo/tuner/RandomTuner.h"
#include "Logging.h"

namespace halo {

KnobSet retryLoop(std::unordered_map<KnobSet, float> &Database,
                  KnobSet const& Initial,
                  std::function<KnobSet(KnobSet&&)> &&Generator,
                  unsigned Limit=3) {

  KnobSet KS(Initial);
  for (unsigned Tries = 0; Tries < Limit; ++Tries) {
    KS = Generator(std::move(KS));
    if (Database.count(KS) == 0) // unique?
      break;
  }

  if (Database.count(KS) != 0)  // not unique?
    return KS; // just return this knob set that is already in the DB.

  Database.insert({KS, ConfigManager::MISSING_QUALITY});
  return KS;
}

KnobSet ConfigManager::genRandom(KnobSet const& BaseKnobs, std::mt19937_64 &RNG) {
  return retryLoop(Database, BaseKnobs,
    [&](KnobSet &&KS) {
      return RandomTuner::randomFrom(std::move(KS), RNG);
    }
  );
}

KnobSet ConfigManager::genNearby(KnobSet const& GoodConfig, std::mt19937_64 &RNG, float EnergyLvl) {
  return retryLoop(Database, GoodConfig,
    [&](KnobSet &&KS) {
      return RandomTuner::nearby(std::move(KS), RNG, EnergyLvl);
    }
  );
}

KnobSet ConfigManager::genPrevious(std::mt19937_64 &RNG) {
  size_t Sz = Database.size();
  if (Sz == 0)
    fatal_error("no previous config to return!");

  std::uniform_int_distribution<size_t> Gen(0, Sz-1);
  size_t Chosen = Gen(RNG);

  // sadly O(Sz)
  for (auto I = Database.begin(); I != Database.end(); ++I, --Chosen)
    if (Chosen == 0)
      return I->first;

  fatal_error("lookup failure in genPrevious");
}

} // namespace halo