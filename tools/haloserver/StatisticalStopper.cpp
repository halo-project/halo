#include "halo/tuner/StatisticalStopper.h"

namespace halo {

using VersionMap = std::unordered_map<std::string, CodeVersion>;

bool StatisticalStopper::shouldStop(std::string const& BestLib, VersionMap const& Versions) const {
  // TODO: implement this!
  return false;
}

} // namespace halo