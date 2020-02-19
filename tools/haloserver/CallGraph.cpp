#include "halo/compiler/CallGraph.h"
#include "llvm/ADT/Optional.h"

namespace halo {

using Vertex = CallGraph::Vertex;
using Edge = CallGraph::Edge;
using Graph = CallGraph::Graph;
using VertexID = CallGraph::VertexID;
using EdgeID = CallGraph::EdgeID;


llvm::Optional<VertexID> findVertex(Vertex Vtex, Graph const& Gr) {
  auto Range = boost::vertices(Gr);
  for (auto I = Range.first; I != Range.second; I++)
    if (Gr[*I] == Vtex)
      return *I;

  return llvm::None;
}


llvm::Optional<EdgeID> findEdge(VertexID Src, VertexID Tgt, Graph const& Gr) {
  auto Range = boost::out_edges(Src, Gr);
  for (auto I = Range.first; I != Range.second; I++)
    if (boost::target(*I, Gr) == Tgt)
      return *I;

  return llvm::None;
}


CallGraph::CallGraph() {
  UnknownID = boost::add_vertex("???", Gr);
}


void CallGraph::addCall(Vertex Src, Vertex Tgt) {
  Edge &Edge = Gr[getEdgeID(getVertexID(Src), getVertexID(Tgt))];
  Edge += 1;
}


/// query the call graph for the existence of an edge.
bool CallGraph::hasCall(Vertex Src, Vertex Tgt) const {
  auto MaybeSrc = findVertex(Src, Gr);
  if (!MaybeSrc)
    return false;

  auto MaybeTgt = findVertex(Tgt, Gr);
  if (!MaybeTgt)
    return false;

  auto MaybeEdge = findEdge(MaybeSrc.getValue(), MaybeTgt.getValue(), Gr);
  return MaybeEdge.hasValue();
}


VertexID CallGraph::getVertexID(Vertex Vtex) {
  auto Result = findVertex(Vtex, Gr);
  if (Result)
    return Result.getValue();

  return boost::add_vertex(Vtex, Gr);
}


EdgeID CallGraph::getEdgeID(VertexID Src, VertexID Tgt) {
  auto Result = findEdge(Src, Tgt, Gr);
  if (Result)
    return Result.getValue();

  return boost::add_edge(Src, Tgt, Gr).first;
}


void CallGraph::dumpDOT(std::ostream &out) const {
  // NOTE: unfortunately we can't use boost::write_graphviz
  // found in boost/graph/graphviz.hpp because it relies on RTTI

  out << "---\ndigraph G {\n";

  auto VRange = boost::vertices(Gr);
  for (auto I = VRange.first; I != VRange.second; I++) {
    auto VertexID = *I;

    // output vertex and its metadata
    out << VertexID
        << " [label=\"" << Gr[VertexID] << "\""
        // << ",style=\""  << Style << "\""
        << "];";

    out << "\n";
  }

  auto ERange = boost::edges(Gr);
  for (auto I = ERange.first; I != ERange.second; I++) {
    auto EdgeID = *I;

    // output edge
    out << boost::source(EdgeID, Gr)
        << " -> "
        << boost::target(EdgeID, Gr)
        << " [label=\"" << Gr[EdgeID]
        << "\"];\n";
  }

  out << "}\n---\n";
}

} // end namespace halo