#pragma once

#include "llvm/ADT/Optional.h"
#include "halo/compiler/CallingContextTree.h"

#include <utility>
#include <list>

namespace halo {

class ClientSession;

class Profiler {
public:
  using ClientList = std::list<std::unique_ptr<ClientSession>>;
  using TuningSection = std::pair<std::string, bool>; // FIXME: maybe one patchable function
                                                      // and a set of reachable funs

  /// updates the profiler with new performance data found in the clients
  void consumePerfData(ClientList &);

  /// advances the age of the profiler's data by one time-step.
  void decay();

  /// @returns the 'best' tuning section
  llvm::Optional<TuningSection> getBestTuningSection();

  void dump(llvm::raw_ostream &);

private:
  CallingContextTree CCT;

};

} // end namespace halo
