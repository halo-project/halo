#include "halo/tuner/ConfigManager.h"
#include "halo/tuner/RandomTuner.h"
#include "Logging.h"

namespace halo {

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

  Database.insert({KS, {}});
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
      if (Chosen == 0 && (!ExcludeTop || !I->second.BeenInTop))
        return I->first;

    MaxTries--;
  } while (MaxTries > 0);

  warning("lookup failure in genPrevious. returning an arbitrary previous config");
  return I->first;
}

} // namespace halo