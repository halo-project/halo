#pragma once

#include "boost/graph/adjacency_list.hpp"
#include "llvm/ADT/Optional.h"
#include <ostream>

namespace halo {

class CodeRegionInfo;
class PerformanceData;
class FunctionInfo;
namespace pb {
  class RawSample;
}

class VertexInfo : public std::string {
public:

  VertexInfo() : std::string() {}
  VertexInfo(std::string const& s) : std::string(s) {}

  // a short name that describes this vertex suitable
  // for dumping to a DOT file as the vertex's label.
  std::string& getDOTLabel() {
    return *this;
  }

  // the full name of the function represented by this vertex.
  std::string const& getFuncName() const {
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

  CallingContextTree();

private:
  // Creates a fresh, unique vertex based on the given FunctionInfo
  VertexID addVertex(FunctionInfo const*);

  // Inserts the data from this sample into the CCT
  void insertSample(CodeRegionInfo const&, pb::RawSample const&);

  Graph Gr;
  VertexID RootVertex;
};

} // end namespace halo