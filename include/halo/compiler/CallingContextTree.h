#pragma once

#include "boost/graph/adjacency_list.hpp"
#include "llvm/ADT/Optional.h"
#include "halo/nlohmann/json_fwd.hpp"
#include <ostream>
#include <map>

namespace halo {

class CodeRegionInfo;
class PerformanceData;
class FunctionInfo;
class Ancestors;
class CallGraph;
namespace pb {
  class RawSample;
}

/// A set of functions with a distinguished "root" function.
/// All functions in the group are reachable from the root, according
/// to the edges in the original program's call graph.
struct FunctionGroup {
  std::string Root; // the name of a patchable function serving as the root of this tuning section.
  std::unordered_set<std::string> AllFuncs; // all functions that make up the tuning section, including the root.

  FunctionGroup(std::string RootFunc) : Root(RootFunc) {
    AllFuncs.insert(RootFunc);
  }
};


// carries some metadata about the last sample seen by a vertex
// in a manner that is more specific than than its calling context.
class CCTNodeInfo {
  public:
    uint64_t Timestamp{0}; // last timestamp seen
    size_t SamplesSeen{0};
    float Hotness{0};
    float IPC{0};
};

using ClientID = size_t;

struct LearningParameters {
  LearningParameters(nlohmann::json const& Config);

  // (0, 1), learning rate / incremental update factor "alpha"
  const float IPC_DISCOUNT;
  const float COOLDOWN_DISCOUNT;

  const float HOTNESS_SAMPLED_IP;
  const float HOTNESS_BOOST;
};

/// Each node summarizes context-sensitive profiling information
/// within a CallingContextTree. It represents a single function,
/// which can have multiple implementations (i.e., libraries).
class VertexInfo {
public:

  VertexInfo() {}
  VertexInfo(LearningParameters const* lp, std::string const& name) : FuncName(name), LP(lp) {}
  VertexInfo(LearningParameters const* lp, std::shared_ptr<FunctionInfo>);

  // a short name that describes this vertex suitable
  // for dumping to a DOT file as the vertex's label.
  std::string getDOTLabel() const;

  // the full name of the function represented by this vertex.
  std::string const& getFuncName() const {
    return FuncName;
  }

  /// Assuming the given sample is contextually
  /// relevant for this vertex, `observerSampleedIP`
  /// will merge the performance metrics from
  /// the sample with existing metrics in this vertex.
  /// It should be used when the sampled IP was observed
  /// at this function context
  void observeSampledIP(ClientID, std::string const& Library, pb::RawSample const&, uint64_t SamplePeriod);

  /// Should be used to indicate that this function context
  /// was recently active in the given RawSample, but NOT as
  /// the sampled IP.
  void observeRecentlyActive(ClientID, std::string const& Library, pb::RawSample const&, uint64_t SamplePeriod);

  // causes the information in this sample to decay
  void decay();

  // a specific measure of hotness for a library.
  // if NONE is provided, then it's a general measure
  float getHotness(llvm::Optional<std::string> Lib) const;

  // a specific measure of the instructions-per-cycle for a library.
  // this currently takes the average across all contexts where the
  // library was called. If NONE is provided, then it's a general measure.
  float getIPC(llvm::Optional<std::string> Lib) const;

  // a specific count of samples seen at this node for a library
  // a sample is "seen" if the IPC was updated. If NONE, then it's a general measure
  size_t getSamplesSeen(llvm::Optional<std::string> Lib) const;

  bool isPatchable() const { return Patchable; }

private:
  std::string FuncName{"<XXX>"};
  bool Patchable{false};

  using KeyType = std::tuple<ClientID, uint32_t, std::string>;
  std::map<KeyType, CCTNodeInfo> SpecificInfo;
  CCTNodeInfo GeneralInfo;

  LearningParameters const* LP{nullptr};

  void observeSample(CCTNodeInfo &Info, pb::RawSample const& RS, uint64_t Period, float HotnessNudge);

  void filterByLib(std::string const& Lib, std::function<void(CCTNodeInfo const&)> Action) const;
}; // end class


/// maintains information about a specific call-edge in the CCT
class EdgeInfo {
public:
  EdgeInfo() {}

  // records a call having occurred along this edge.
  void observe();

  /// returns a score indicating how often this branch / call has
  /// happened recently
  auto getFrequency() const { return Frequency; }

  // causes the information in this sample to decay
  void decay(float CooldownRate);

  private:
    float Frequency{0};
}; // end class


// Profiling-based attributes of a function group.
struct GroupPerf {
  double Hotness{0};
  double IPC{0};
  size_t SamplesSeen{0};
};


/// A container for context-sensitive profiling data.
///
/// Based on the CCT described by by Ammons, Ball, and Larus in
/// "Exploiting Hardware Performance Counters with Flow and
/// Context Sensitive Profiling" in PLDI '97
class CallingContextTree {
public:
  using Graph = boost::adjacency_list<
                  boost::vecS, boost::vecS, boost::bidirectionalS,
                  VertexInfo, EdgeInfo>;
  using VertexID = Graph::vertex_descriptor;
  using EdgeID = Graph::edge_descriptor;

  /// adds the given profiling data to the tree
  void observe(CallGraph const&, ClientID, CodeRegionInfo const&, PerformanceData const&);

  /// causes the data in this tree to age by one time-step.
  void decay();

  /// a functional-style fold operation applied to vertices
  /// in an arbitrary order. You'll need to manually instantiate versions
  /// with your particular AccTy in the .cpp file.
  template <typename AccTy>
  AccTy reduce(std::function<AccTy(VertexID, VertexInfo const&, AccTy)>, AccTy Initial) const;

  /// returns the root vertex's ID
  VertexID getRoot() { return RootVertex; }

  /// obtains the vertex info for the context represented by the
  /// vertex. The context is a sequence of functions from the root
  /// to the given vertex, but the root is _not_ included in the
  /// returned sequence since it is not a "real" vertex.
  std::vector<VertexID> contextOf(VertexID);

  VertexInfo const& getInfo(VertexID) const;

  // returns the sum of the weights of all incoming edges to this
  // CCT node. Those weights represent recent calls to this node that have occurred.
  float getCallFrequency(VertexID) const;

  /// If Tgt is reachable from Src, then all paths connecting them
  /// is returned.
  ///
  /// A "path" is a sequence of distinct vertices to visit, starting from Start.
  /// For example, if the graph is
  ///
  ///     A -> B
  ///     B -> B
  ///     B -> C
  ///
  /// then a path from A to C is: [A, B, C]
  std::list<std::list<VertexID>> allPaths(VertexID Start, std::shared_ptr<FunctionInfo> const& Tgt) const;

  /// Returns a shortest path from Start to a vertex matching Tgt.
  /// Ties are broken by summing the hotness of all vertices in each
  /// path and choosing the path with more hotness.
  /// If two equally short paths have equal hotness, then an arbitrary path is chosen.
  ///
  /// A "path" is a sequence of distinct vertices to visit, starting from Start.
  ///
  /// @returns llvm::None if no path exists. Otherwise a path [Start .. Tgt]
  llvm::Optional<std::list<VertexID>> shortestPath(VertexID Start, std::shared_ptr<FunctionInfo> const& Tgt) const;

  /// Gather some performance information about this function group. Optionally, for a specific library version
  GroupPerf currentPerf(FunctionGroup const& FuncGroup, llvm::Optional<std::string> Lib);

  /// dumps the graph in DOT format
  void dumpDOT(std::ostream &);

  // returns true iff the context tree is currently malformed.
  bool isMalformed() const;

  CallingContextTree(LearningParameters const* lp, uint64_t samplePeriod);

private:

  // Inserts the data from this sample into the CCT
  void insertSample(CallGraph const&, ClientID, CodeRegionInfo const&, pb::RawSample const&);

  // Inserts branch-sample data starting at the given vertex into the CCT.
  void walkBranchSamples(ClientID, Ancestors&, CallGraph const&, VertexID, CodeRegionInfo const&, pb::RawSample const&);

  Graph Gr;
  VertexID RootVertex;
  uint64_t SamplePeriod;
  LearningParameters const* LP;
};

} // end namespace halo