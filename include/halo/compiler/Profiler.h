#pragma once

#include "llvm/ADT/Optional.h"
#include "halo/compiler/CallingContextTree.h"
#include "halo/compiler/CallGraph.h"

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

  /// @returns true iff the profiler knows we have bitcode available
  ///          for the given function.
  bool haveBitcode(std::string const& Name) { return FuncsWithBitcode.count(Name) != 0; }

  /// modifies knowledge of whether we have bitcode for the given function.
  void setBitcodeStatus(std::string const& Name, bool Status);

  // obtains the profiler's static call graph.
  CallGraph& getCallGraph() { return CG; }

  void dump(llvm::raw_ostream &);

private:
  CallingContextTree CCT;
  CallGraph CG;
  std::set<std::string> FuncsWithBitcode;

};

} // end namespace halo
