#include "halo/tuner/StatisticalStopper.h"
#include "halo/tuner/ConfigManager.h"

namespace halo {

using VersionMap = std::unordered_map<std::string, CodeVersion>;

bool StatisticalStopper::shouldStop(std::string const& BestLib,
                                    VersionMap const& Versions,
                                    ConfigManager const& Manager) const {


  double ConfigsCompiled = 0, UniqueConfigs = 0;
  for (auto const& Elm : Versions) {
    UniqueConfigs++;
    ConfigsCompiled += Elm.second.getConfigs().size();
  }

  // unique / compiled == x / N
  // <=> (n*unique)/compiled == x
  double TotalUnique = (N * UniqueConfigs) / ConfigsCompiled;

  double NumQualityEstimates = Manager.size();

  clogs() << "shouldStop:"
          << "\n\ttotal space sz = " << N
          << "\n\tconfigs compiled = " << ConfigsCompiled
          << "\n\tunique configs = " << UniqueConfigs
          << "\n\ttotal unique sz = " << TotalUnique
          << "\n\tpct of unique checked = " << 100.0 * (UniqueConfigs / TotalUnique) << "%"
          << "\n\tpct total config checked = " << 100.0 * (NumQualityEstimates / N) << "%"
          << "\n";

  // TODO: implement this!
  return false;
}

} // namespace halo