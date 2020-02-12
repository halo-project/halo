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

  /// updates the profiler with new performance data found in the clients
  void consumePerfData(ClientList &);

  /// advances the age of the profiler's data by one time-step.
  void decay();

  /// @returns the hottest calling context (i.e., a sequence of functions).
  /// the context is ordered from hottest-function -> caller -> caller -> root
  /// ties are broken arbitrarily.
  llvm::Optional<std::vector<VertexInfo>> getHottestContext();

  void dump(llvm::raw_ostream &);

private:
  CallingContextTree CCT;

};

} // end namespace halo
