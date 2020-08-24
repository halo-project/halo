#pragma once

#include "boost/graph/adjacency_list.hpp"
#include "llvm/ADT/Optional.h"
#include <set>
#include <list>

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

// Metadata about an edge between functions F --> G.
// We record information about the types of callsites in F that refer to G.
struct CGEdge {
  size_t LoopBodyCallsites = 0;
  size_t OtherCallsites = 0;

  size_t totalCallsites() const { return LoopBodyCallsites + OtherCallsites; }

  void addCallsite(bool WithinLoopBody) {
    if (WithinLoopBody)
      LoopBodyCallsites += 1;
    else
      OtherCallsites += 1;
  }

};

/// A simple static call graph, which is a
/// directed graph where an edge A->B represents
/// the fact that A could call B.
///
/// We annotate edges with the number of call-sites
/// in A that refer to B.
class CallGraph {
public:
  using Vertex = CGVertex;
  using Edge = CGEdge;
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
  void addNode(std::string const& Name, bool HaveBitcode, bool HintedRoot);

  // Records the existence of a call-site
  // within the function Src that calls Tgt.
  // The bool indicates whether the call-site is within a loop
  // of Src.
  void addCall(Vertex Src, Vertex Tgt, bool WithinLoopBody);

  // obtains all vertices marked as a hinted tuning section root.
  std::set<Vertex> const& getHintedRoots() const;

  /// query the call graph for the existence of an edge.
  /// @return true iff Src exists and contains a call-site to Tgt.
  bool hasCall(Vertex Src, Vertex Tgt) const;

  // returns a copy of the Edge information, if such an edge exists.
  llvm::Optional<Edge> getCallEdge(Vertex Src, Vertex Tgt) const;

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
  std::set<Vertex> allReachable(Vertex src) const;

  // returns a set of all paths from src to tgt.
  std::list<std::list<Vertex>> allPaths(Vertex src, Vertex tgt) const;

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

  std::set<Vertex> HintedRoots;
};

} // end namespace halo