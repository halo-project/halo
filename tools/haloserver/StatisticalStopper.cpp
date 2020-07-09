#include "halo/tuner/StatisticalStopper.h"
#include "halo/tuner/ConfigManager.h"
#include "halo/compiler/CodeRegionInfo.h"

namespace halo {

using VersionMap = std::unordered_map<std::string, CodeVersion>;

// TODO: this doesn't actually implement Vuduc et al's decision procedure.
// The reason is that the space is so huge that even 1% of it would take
// a very long time to check. All of the spaces checked in the paper
// were on the order of 5% or higher.
bool StatisticalStopper::shouldStop(std::string const& BestLib,
                                    VersionMap const& Versions,
                                    ConfigManager const& Manager) const {


  // float IPCOrig = Versions.at(CodeRegionInfo::OriginalLib).getIPC().mean();
  // float IPCCurrent = Versions.at(BestLib).getIPC().mean();

  // float Diff = IPCCurrent - IPCOrig;
  // if (Diff / IPCOrig > 0.2)
  //   return true; // stop at 20% better.


  double ConfigsCompiled = 0, UniqueConfigs = 0;
  for (auto const& Elm : Versions) {
    UniqueConfigs++;
    ConfigsCompiled += Elm.second.getConfigs().size();
  }

  // unique / compiled == x / N
  // <=> (n*unique)/compiled == x
  double TotalUnique = (N * UniqueConfigs) / ConfigsCompiled;

  double NumQualityEstimates = Manager.size();
  double UniqueCompileProbability = (UniqueConfigs / ConfigsCompiled);

  clogs() << "shouldStop:"
          << "\n\ttotal space sz = " << N
          << "\n\tconfigs compiled = " << ConfigsCompiled
          << "\n\tunique configs = " << UniqueConfigs
          << "\n\tunique compile probability = " << UniqueCompileProbability
          << "\n\ttotal unique sz = " << TotalUnique
          << "\n\tpct of unique checked = " << 100.0 * (UniqueConfigs / TotalUnique) << "%"
          << "\n\tpct total config checked = " << 100.0 * (NumQualityEstimates / N) << "%"
          << "\n";

  double minimumSuccessProb = 0.01; // 1%
  return UniqueCompileProbability < minimumSuccessProb;
}

} // namespace halo