#pragma once

#include "boost/graph/adjacency_list.hpp"
#include "llvm/ADT/Optional.h"
#include <ostream>
#include <map>

namespace halo {

class CodeRegionInfo;
class PerformanceData;
class FunctionInfo;
namespace pb {
  class RawSample;
}

using ClientID = size_t;

/// Each node summarizes context-sensitive profiling information
/// within a CallingContextTree.
class VertexInfo {
public:

  VertexInfo() {}
  VertexInfo(std::string const& name) : FuncName(name) {}
  VertexInfo(FunctionInfo const*);

  // a short name that describes this vertex suitable
  // for dumping to a DOT file as the vertex's label.
  std::string getDOTLabel() const;

  // the full name of the function represented by this vertex.
  std::string const& getFuncName() const {
    return FuncName;
  }

  /// Assuming the given sample is contextually
  /// relevant for this vertex, `observerSample`
  /// will merge the performance metrics from
  /// the sample with existing metrics in this vertex.
  void observeSample(ClientID, pb::RawSample const&);

  // causes the information in this sample to decay
  void decay();

  // a measure of how often samples appeared in this function.
  auto getHotness() const { return Hotness; }

  bool isPatchable() const { return Patchable; }

private:
  std::string FuncName{"<XXX>"};
  bool Patchable{false};
  float Hotness{0};
  std::map<std::pair<ClientID, uint32_t>, uint64_t> LastSampleTime; // thread_id -> timestamp

  static const float HOTNESS_BASELINE;
  static const float HOTNESS_DISCOUNT;
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
                  VertexInfo>;
  using VertexID = Graph::vertex_descriptor;

  void observe(ClientID, CodeRegionInfo const&, PerformanceData const&);

  // causes the perf data in this tree to age.
  void decay();

  // dumps the graph in DOT format
  void dumpDOT(std::ostream &);

  CallingContextTree();

private:

  // Inserts the data from this sample into the CCT
  void insertSample(ClientID, CodeRegionInfo const&, pb::RawSample const&);

  Graph Gr;
  VertexID RootVertex;
};

} // end namespace halo