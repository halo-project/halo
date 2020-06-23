#pragma once

#include "boost/graph/adjacency_list.hpp"
#include <set>

namespace halo {

struct CGVertex {
  // NOTE: the assumption that we do not have the bitcode, by default, is sort-of baked into
  // the implementation of the ProgramInfoPass, so be careful changing that.
  CGVertex() : Name(""), HaveBitcode(false) {}
  CGVertex(const char* name, bool haveBitcode=false) : Name(name), HaveBitcode(haveBitcode) {}
  CGVertex(std::string const& name, bool haveBitcode=false) : Name(name), HaveBitcode(haveBitcode) {}
  std::string Name;
  bool HaveBitcode; // irrelevant for node comparisons; it's just extra metadata!
};

bool operator==(CGVertex const& A, CGVertex const& B);
bool operator<(CGVertex const& A, CGVertex const& B);

/// A simple static call graph, which is a
/// directed graph where an edge A->B represents
/// the fact that A could call B.
///
/// We annotate edges with the number of call-sites
/// in A that refer to B.
class CallGraph {
public:
  using Vertex = CGVertex;
  using Edge = size_t;
  using Graph = boost::adjacency_list<
                  boost::vecS, boost::vecS, boost::bidirectionalS,
                  Vertex, Edge>;
  using VertexID = Graph::vertex_descriptor;
  using EdgeID = Graph::edge_descriptor;

  /// Returns the vertex representing an "unknown"
  /// or "external" function that the analysis was
  /// unable to determine.
  Vertex const& getUnknown() const { return Gr[UnknownID]; }

  // tries to add the call-graph node. If the node already exists, then
  // its bitcode status is updated to the given status.
  void addNode(std::string const& Name, bool HaveBitcode);

  // Records the existence of a call-site
  // within the function Src that calls Tgt.
  void addCall(Vertex Src, Vertex Tgt);

  /// query the call graph for the existence of an edge.
  /// @return true iff Src exists and contains a call-site to Tgt.
  bool hasCall(Vertex Src, Vertex Tgt) const;

  // query the call graph to see if this function could call either:
  //  1. the unknown function, i.e., a non-analyzable callee.
  //  2. a function for which no bitcode definition exists (like libm's sin, tan, etc.)
  bool hasOpaqueCall(Vertex Src) const;

  // returns true iff the given function is contained in the CG and
  // makes no calls i.e., its out-edge set in the CG is empty.
  bool isLeaf(Vertex Func) const;

  /// returns the set of all vertices reachable from the given vertex,
  /// i.e., all functions that can be called, starting from Src.
  ///
  /// The set will always at least contain the Src itself, unless if
  /// the Src is not in the graph!
  std::set<Vertex> allReachable(Vertex src);

  // returns true iff the call-graph has a node for the given function
  bool contains(Vertex Func) const;

  // returns true iff the function, if it exists, is known to have bitcode available.
  bool haveBitcode(std::string const& Func) const;

  void dumpDOT(std::ostream &out) const;

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