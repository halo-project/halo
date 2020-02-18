#pragma once

#include "boost/graph/adjacency_list.hpp"

namespace halo {

/// A simple static call graph, which is a
/// directed graph where an edge A->B represents
/// the fact that A could call B.
///
/// We annotate edges with the number of call-sites
/// in A that refer to B.
class CallGraph {
public:
  using Vertex = std::string;
  using Edge = size_t;
  using Graph = boost::adjacency_list<
                  boost::vecS, boost::vecS, boost::bidirectionalS,
                  Vertex, Edge>;
  using VertexID = Graph::vertex_descriptor;
  using EdgeID = Graph::edge_descriptor;

  /// Returns the vertex representing an "unknown"
  /// or "external" function that the analysis was
  /// unable to determine.
  Vertex const& getUnknown() { return Gr[UnknownID]; }

  // Records the existence of a call-site
  // within the function Src that calls Tgt.
  void addCall(Vertex Src, Vertex Tgt);

  void dumpDOT(std::ostream &out);

  CallGraph();

private:

  /// obtains and/or creates the vertex
  VertexID getVertexID(Vertex);

  /// obtains and/or creates the edge
  EdgeID getEdgeID(VertexID Src, VertexID Tgt);

  Graph Gr;
  VertexID UnknownID;
};

} // end namespace halo