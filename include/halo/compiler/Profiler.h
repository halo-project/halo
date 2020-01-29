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

  /// @returns the name of the most sampled function and whether it is patchable.
  /// patchable functions are prioritized over non-patchable and will always be returned if sampled.
  llvm::Optional<std::pair<std::string, bool>> getMostSampled(ClientList &);

  void dump(llvm::raw_ostream &);

private:
  CallingContextTree CCT;

};

} // end namespace halo
