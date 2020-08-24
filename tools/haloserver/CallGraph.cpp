#include "halo/compiler/CallGraph.h"
#include "halo/compiler/ReachableVisitor.h"

#include <unordered_set>


namespace halo {

using Vertex = CallGraph::Vertex;
using Edge = CallGraph::Edge;
using Graph = CallGraph::Graph;
using VertexID = CallGraph::VertexID;
using EdgeID = CallGraph::EdgeID;

// bitcode status is irrelevant to node equality!
bool operator==(CGVertex const& A, CGVertex const& B) {
  return A.Name == B.Name;
}

// bitcode status is irrelevant to node ordering!
bool operator<(CGVertex const& A, CGVertex const& B) {
  return A.Name < B.Name;
}


// NOTE: this is an O(V) operation right now!
llvm::Optional<VertexID> findVertex(Vertex Vtex, Graph const& Gr) {
  auto Range = boost::vertices(Gr);
  for (auto I = Range.first; I != Range.second; I++)
    if (Gr[*I] == Vtex)
      return *I;

  return llvm::None;
}

// NOTE: this is an O(E) operation right now!
llvm::Optional<EdgeID> findEdge(VertexID Src, VertexID Tgt, Graph const& Gr) {
  auto Range = boost::out_edges(Src, Gr);
  for (auto I = Range.first; I != Range.second; I++)
    if (boost::target(*I, Gr) == Tgt)
      return *I;

  return llvm::None;
}


CallGraph::CallGraph() {
  UnknownID = boost::add_vertex(CGVertex("???", false), Gr);
}


void CallGraph::addCall(Vertex Src, Vertex Tgt, bool WithinLoopBody) {
  Edge &Edge = Gr[getEdgeID(getVertexID(Src), getVertexID(Tgt))];
  Edge.addCallsite(WithinLoopBody);
}

std::set<Vertex> const& CallGraph::getHintedRoots() const {
  return HintedRoots;
}

llvm::Optional<Edge> CallGraph::getCallEdge(Vertex Src, Vertex Tgt) const {
  auto MaybeSrc = findVertex(Src, Gr);
  if (!MaybeSrc)
    return llvm::None;

  auto MaybeTgt = findVertex(Tgt, Gr);
  if (!MaybeTgt)
    return llvm::None;

  auto MaybeEdgeID = findEdge(MaybeSrc.getValue(), MaybeTgt.getValue(), Gr);
  if (!MaybeEdgeID)
    return llvm::None;

  return Gr[MaybeEdgeID.getValue()];
}


/// query the call graph for the existence of an edge.
bool CallGraph::hasCall(Vertex Src, Vertex Tgt) const {
  return getCallEdge(Src, Tgt).hasValue();
}

bool CallGraph::hasOpaqueCall(Vertex Src) const {
  auto MaybeID = findVertex(Src, Gr);
  if (!MaybeID)
    return false; // function doesn't exist, so we can't confirm this fact!

  VertexID VID = MaybeID.getValue();

  // first case: we have an edge to the unknown vertex.
  if (findEdge(VID, UnknownID, Gr))
    return true;

  // search its callees for a function with NO bitcode
  auto Range = boost::out_edges(VID, Gr);
  for (auto I = Range.first; I != Range.second; I++)
    if (Gr[boost::target(*I, Gr)].HaveBitcode == false)
      return true;  // second case: a callee with no bitcode.

  return false;
}

void CallGraph::addNode(std::string const& Name, bool HaveBitcode, bool HintedRoot) {
  VertexID VID = UnknownID;
  Vertex V(Name, HaveBitcode);

  // sadly, Vertex::operator== was implemented to ignore bitcode setting differences,
  // and the way addNode is designed was to mutate/update once we discover
  // that the function does actually have bitcode. So, to make sure the set
  // has fresh and correct vertex data, we add and remove. it looks odd but is needed :/
  // FIXME: more dissertation rush hacking!
  if (HaveBitcode && HintedRoot) {
    HintedRoots.erase(V);
    HintedRoots.insert(V);
  }

  auto MaybeID = findVertex(V, Gr);
  if (!MaybeID)
    VID = boost::add_vertex(V, Gr);
  else
    VID = MaybeID.getValue();

  Gr[VID].HaveBitcode = HaveBitcode;
}

bool CallGraph::haveBitcode(std::string const& Func) const {
  auto MaybeID = findVertex(Func, Gr);
  if (!MaybeID)
    return false;

  return Gr[MaybeID.getValue()].HaveBitcode;
}

bool CallGraph::isLeaf(Vertex Func) const {
  auto MaybeFunc = findVertex(Func, Gr);
  if (!MaybeFunc)
    return false;

  auto Range = boost::out_edges(MaybeFunc.getValue(), Gr);
  return Range.first == Range.second;
}


bool CallGraph::contains(Vertex Func) const {
  return findVertex(Func, Gr).hasValue();
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

std::set<Vertex> CallGraph::allReachable(Vertex Root) const {
  std::set<Vertex> Reachable;

  if (!contains(Root))
    return Reachable;

  Reachable.insert(Root); // can always reach self, since it's in the graph.

  // perform the search
  VertexID RootID = findVertex(Root, Gr).getValue();
  std::unordered_set<VertexID> ReachableIDs;
  ReachableVisitor<Graph> CV(ReachableIDs, RootID);
  // https://www.boost.org/doc/libs/1_73_0/libs/graph/doc/bgl_named_params.html
  boost::depth_first_search(Gr, boost::visitor(CV).root_vertex(RootID));

  // convert id -> vertex
  for (auto ID : ReachableIDs)
    Reachable.insert(Gr[ID]);

  return Reachable;
}

std::list<std::list<Vertex>> CallGraph::allPaths(Vertex Src, Vertex Tgt) const {
  std::list<std::list<Vertex>> Paths;

  if (!contains(Src) || !contains(Tgt))
    return Paths;

  class PathSearch : public boost::default_dfs_visitor {
  public:
    PathSearch(VertexID SrcID, Vertex const& tgt, std::list<std::list<VertexID>>& p)
      : Source(SrcID), Target(tgt), Paths(p) {}

    void start_vertex(VertexID u, const Graph& g) {
      if (Source == u)
        CurPath.push_back(u);
    }

    // invoked on each vertex at the start of DFS-VISIT
    void discover_vertex(VertexID u, const Graph& g) {
      if (CurPath.front() != Source)
        return; // then we're not in the sub-tree

      if (Source == u)
        return; // we've already accounted for it in start_vertex

      CurPath.push_back(u);

      // we've found a new path?
      auto const& UInfo = g[u];
      if (Target == UInfo)
        Paths.push_back(CurPath);
    }

    void finish_vertex(VertexID u, const Graph& g) {
      if (CurPath.front() != Source)
        return;

      CurPath.pop_back();
    }

  private:
    VertexID Source;
    Vertex const& Target;
    std::list<std::list<VertexID>>& Paths;
    std::list<VertexID> CurPath;
  }; // end class

  // search for all paths
  VertexID SrcID = findVertex(Src, Gr).getValue();
  std::list<std::list<VertexID>> PathIDs;
  PathSearch PS(SrcID, Tgt, PathIDs);
  // https://www.boost.org/doc/libs/1_73_0/libs/graph/doc/bgl_named_params.html
  boost::depth_first_search(Gr, boost::visitor(PS).root_vertex(SrcID));

  // convert IDs to Vertices
  for (auto const& IDPath : PathIDs) {
    std::list<Vertex> Path;

    for (auto ID : IDPath)
      Path.push_back(Gr[ID]);

    Paths.push_back(Path);
  }

  return Paths;
}


void CallGraph::dumpDOT(std::ostream &out) const {
  // NOTE: unfortunately we can't use boost::write_graphviz
  // found in boost/graph/graphviz.hpp because it relies on RTTI

  out << "---\ndigraph G {\n";

  auto VRange = boost::vertices(Gr);
  for (auto I = VRange.first; I != VRange.second; I++) {
    auto VertexID = *I;
    auto const& Vertex = Gr[VertexID];

    auto Style = Vertex.HaveBitcode ? "solid" : "dashed";

    // output vertex and its metadata
    out << VertexID
        << " [label=\"" << Vertex.Name << "\""
        << ",style=\""  << Style << "\""
        << "];";

    out << "\n";
  }

  auto ERange = boost::edges(Gr);
  for (auto I = ERange.first; I != ERange.second; I++) {
    auto EdgeID = *I;
    auto const& Edge = Gr[EdgeID];

    // output edge
    out << boost::source(EdgeID, Gr)
        << " -> "
        << boost::target(EdgeID, Gr)
        << " [label=\"" << Edge.totalCallsites() << ";lp=" << Edge.LoopBodyCallsites
        << "\"];\n";
  }

  out << "}\n---\n";
}

} // end namespace halo