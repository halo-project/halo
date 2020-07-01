#include "halo/tuner/StatisticalStopper.h"
#include "halo/tuner/ConfigManager.h"

namespace halo {

using VersionMap = std::unordered_map<std::string, CodeVersion>;

bool StatisticalStopper::shouldStop(std::string const& BestLib,
                                    VersionMap const& Versions,
                                    ConfigManager const& Manager) const {

  double Pct = Manager.size() / N;
  clogs() << "shouldStop: precent of space (sz = " << N << ") searched: "
          << 100.0 * Pct
          << "\n";

  // TODO: implement this!
  return false;
}

} // namespace halo