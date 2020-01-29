#pragma once

#include "llvm/ADT/Optional.h"

#include <utility>
#include <list>

namespace halo {

class ClientSession;

class Profiler {
public:
  /// @returns the name of the most sampled function and whether it is patchable.
  /// patchable functions are prioritized over non-patchable and will always be returned if sampled.
  llvm::Optional<std::pair<std::string, bool>> getMostSampled(std::list<std::unique_ptr<ClientSession>> &Clients);

  void dump(llvm::raw_ostream &);

};

} // end namespace halo
