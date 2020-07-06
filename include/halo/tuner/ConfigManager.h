#pragma once

#include "halo/tuner/KnobSet.h"
#include <random>
#include <list>
#include <unordered_map>

namespace halo {

class ConfigManager {
public:

  static constexpr float MISSING_QUALITY = std::numeric_limits<float>::min();

  // returns the size of the top-configs buffer.
  size_t sizeTop() const {
    return Top.size();
  }

  // adds a config to the end of the top-configs buffer.
  void addTop(KnobSet const& KS) {
    Top.push_back(KS);
    Database[KS].BeenInTop = true;
  }

  // removes the first config from the top-configs buffer.
  // raises an error if it's empty.
  KnobSet popTop() {
    KnobSet KS = Top.front();
    Top.pop_front();
    return KS;
  }

  // generates a (usually unique) random knob configuration, based on the given one
  KnobSet genRandom(KnobSet const& BaseKnobs, std::mt19937_64 &RNG);

  // generates a (usually unique) nearby knob configuration, based on the given one
  KnobSet genNearby(KnobSet const& GoodConfig, std::mt19937_64 &RNG, float EnergyLvl);

  // randomly picks a previously generated knob and returns it. If the
  // manager is empty, raises an error.
  // ExcludeTop means that the configuration returned will be one that has
  // not already been enqueued into the Top queue.
  KnobSet genPrevious(std::mt19937_64 &RNG, bool ExcludeTop=true);

  // get a knob set corresponding to a compiler-writer's opinion
  // of what might be good to try next. Returns None when it's
  // out of ideas.
  llvm::Optional<KnobSet> genExpertOpinion(KnobSet const& BaseKnobs);

  void setPredictedQuality(KnobSet const& KS, float Quality) {
    Database[KS].IPC = Quality;
  }

  // returns ConfigManager::MISSING_QUALITY for unseen KnobSets.
  float getPredictedQuality(KnobSet const& KS) const {
    if (Database.count(KS) == 0)
      return MISSING_QUALITY;

    return Database.at(KS).IPC;
  }

  size_t size() const { return Database.size(); }

  auto begin() noexcept { return Database.begin(); }
  auto begin() const noexcept { return Database.cbegin(); }

  auto end() noexcept { return Database.end(); }
  auto end() const noexcept { return Database.cend(); }

private:

  struct Metadata {
    float IPC = ConfigManager::MISSING_QUALITY;
    bool BeenInTop = false;
  };

  KnobSet retryLoop(KnobSet const& Initial,
                  std::function<KnobSet(KnobSet&&)> &&Generator,
                  unsigned Limit=3);

  void insert(KnobSet const&);

  std::list<KnobSet> Top;
  std::unordered_map<KnobSet, Metadata> Database;
  unsigned Opines = 0;
};

} // namespace halo