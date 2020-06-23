#pragma once

#include "llvm/ADT/Optional.h"
#include "halo/compiler/CallingContextTree.h"
#include "halo/compiler/CallGraph.h"
#include "halo/nlohmann/json_fwd.hpp"

#include <utility>
#include <list>

using JSON = nlohmann::json;

namespace halo {

class ClientSession;

/// Tracks all profiling information.
class Profiler {
public:
  using ClientList = std::list<std::unique_ptr<ClientSession>>;
  using CCTNode = CallingContextTree::VertexID;

  Profiler(JSON const&);

  /// given a set of functions that form sub-trees of the CCT,
  /// returns an IPC rating for the entire group, optionally, specific to a library.
  GroupPerf currentPerf(FunctionGroup const&, llvm::Optional<std::string> LibName);

  /// updates the profiler with new performance data found in the clients
  void consumePerfData(ClientList &);

  /// in terms of number of instructions per sample
  uint64_t getSamplePeriod() const {
    return SamplePeriod;
  }

  // returns a count of the total number of samples consumed so far.
  size_t samplesConsumed() const { return SamplesSeen; }

  // returns the 'hottest' CCT node known to the profiler currently.
  llvm::Optional<CCTNode> hottestNode();

  /// At the given CCTNode, it climbs the calling context towards the root
  /// until it finds a good candidate for a tuning section, based on hotness and patchability.
  /// Note that a function is considered an ancestor of itself.
  /// @returns the chosen function's name, if one was found
  llvm::Optional<std::string> findSuitableTuningRoot(Profiler::CCTNode);

  /// advances the age of the profiler's data by one time-step.
  void decay();

  /// @returns true iff the profiler knows we have bitcode available
  ///          for the given function.
  bool haveBitcode(std::string const& Name) const { return CG.haveBitcode(Name); }

  // obtains the profiler's static call graph.
  CallGraph& getCallGraph() { return CG; }

  void dump(llvm::raw_ostream &);

private:
  // Here are some large prime numbers to help deter periodicity:
    //
    //   https://primes.utm.edu/lists/small/millions/
    //
    // We want to avoid having as many divisors as possible in case of
    // repetitive behavior, e.g., a long-running loop executing exactly 323
    // instructions per iteration. There's a (slim) chance we sample the
    // same instruction every time because our period is a multiple of 323.
    // In reality, CPUs have noticable non-constant skid, but we don't want to
    // rely on that for good samples.
  uint64_t SamplePeriod;
  LearningParameters LP;

  CallingContextTree CCT;
  CallGraph CG;
  size_t SamplesSeen{0};

};

} // end namespace halo
