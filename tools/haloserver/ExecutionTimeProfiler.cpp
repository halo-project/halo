#include "halo/compiler/ExecutionTimeProfiler.h"

namespace halo {

void ExecutionTimeProfiler::observe(ClientID ID, CodeRegionInfo const& CRI, std::vector<pb::CallCountData> const& AllData) {
  for (auto const& Item : AllData)
    observeOne(ID, CRI, Item);
}

void ExecutionTimeProfiler::observeOne(ClientID ID, CodeRegionInfo const& CRI, pb::CallCountData const& CCD) {
  fatal_error("todo");
}

} // namespace halo