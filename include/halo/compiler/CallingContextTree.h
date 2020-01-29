#pragma once

#include "boost/graph/adjacency_list.hpp"
#include "llvm/ADT/Optional.h"
#include <ostream>

namespace halo {

class CodeRegionInfo;
class PerformanceData;
class FunctionInfo;

class VertexInfo : public std::string {
public:

  VertexInfo() : std::string() {}
  VertexInfo(std::string const& s) : std::string(s) {}

  // a short name that describes this vertex suitable
  // for dumping to a DOT file as the vertex's label.
  std::string& getDOTLabel() {
    return *this;
  }
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

  void observe(CodeRegionInfo const&, PerformanceData const&);

  // dumps the graph in DOT format
  void dumpDOT(std::ostream &);

private:

  /// Lookup a vertex by name
  llvm::Optional<VertexID> lookup(std::string const&);

  // looks up the vertex, and if it is not found, adds one to the graph
  // using basic information from the FunctionInfo.
  VertexID getOrAddVertex(FunctionInfo const*);

  Graph Gr;
  std::unordered_map<std::string, VertexID> VertexMap;
};

} // end namespace halo