#pragma once

#include "boost/graph/adjacency_list.hpp"
#include "llvm/ADT/Optional.h"
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

// carries some metadata about the last sample seen by a vertex.
class LastSampleInfo {
  public:
    uint64_t Timestamp = 0;
    bool Initial = false;
    float Increment = 0.0f;
};

using ClientID = size_t;

/// Each node summarizes context-sensitive profiling information
/// within a CallingContextTree.
class VertexInfo {
public:

  VertexInfo() {}
  VertexInfo(std::string const& name) : FuncName(name) {}
  VertexInfo(std::shared_ptr<FunctionInfo>);

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
  void observeSampledIP(ClientID, pb::RawSample const&);

  /// Should be used to indicate that this function context
  /// was recently active in the given RawSample, but NOT as
  /// the sampled IP.
  void observeRecentlyActive(ClientID, pb::RawSample const&);

  // causes the information in this sample to decay
  void decay();

  // a measure of how often samples appeared in this function.
  auto getHotness() const { return Hotness; }

  bool isPatchable() const { return Patchable; }

private:
  std::string FuncName{"<XXX>"};
  bool Patchable{false};
  float Hotness{0};
  std::map<std::pair<ClientID, uint32_t>, LastSampleInfo> LastSample;

  static const float HOTNESS_BASELINE;
  static const float HOTNESS_DISCOUNT;
  static const float HOTNESS_INITIAL;
  static const float HOTNESS_BOOST;

  void observeSample(ClientID ID, pb::RawSample const& RS, float Discount, float Initial);
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
  void decay();

  private:
    static const float FREQUENCY_DISCOUNT;
    float Frequency{0};
}; // end class


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
  std::vector<VertexInfo> contextOf(VertexID);

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

  /// dumps the graph in DOT format
  void dumpDOT(std::ostream &);

  // returns true iff the context tree is currently malformed.
  bool isMalformed() const;

  CallingContextTree();

private:

  // Inserts the data from this sample into the CCT
  void insertSample(CallGraph const&, ClientID, CodeRegionInfo const&, pb::RawSample const&);

  // Inserts branch-sample data starting at the given vertex into the CCT.
  void walkBranchSamples(ClientID, Ancestors&, CallGraph const&, VertexID, CodeRegionInfo const&, pb::RawSample const&);

  Graph Gr;
  VertexID RootVertex;
};

} // end namespace halo