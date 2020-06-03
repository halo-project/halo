#pragma once

#include "llvm/ADT/Optional.h"
#include "halo/compiler/CallingContextTree.h"
#include "halo/compiler/CallGraph.h"

#include <utility>
#include <list>

namespace halo {

class ClientSession;

/// Tracks all profiling information.
class Profiler {
public:
  using ClientList = std::list<std::unique_ptr<ClientSession>>;
  using CCTNode = CallingContextTree::VertexID;

  Profiler() : CCT(getSamplePeriod()) {}

  /// updates the profiler with new performance data found in the clients
  void consumePerfData(ClientList &);

  /// in terms of number of instructions per sample
  uint64_t getSamplePeriod() const {
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
    return 15485867;
  }

  // returns a count of the total number of samples consumed so far.
  size_t samplesConsumed() const { return SamplesSeen; }

  // returns the 'hottest' CCT node known to the profiler currently.
  llvm::Optional<CCTNode> hottestNode();

  /// At the given CCTNode, it climbs the calling context towards the root
  /// until it finds the first patchable function, and returns that function's name.
  /// Note that a function is considered an ancestor of itself.
  llvm::Optional<std::string> getFirstPatchableInContext(Profiler::CCTNode);

  /// advances the age of the profiler's data by one time-step.
  void decay();

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
  size_t SamplesSeen{0};
  std::set<std::string> FuncsWithBitcode;

};

} // end namespace halo
