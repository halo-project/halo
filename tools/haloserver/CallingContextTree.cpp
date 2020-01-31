#include "halo/compiler/CallingContextTree.h"
#include "halo/compiler/CodeRegionInfo.h"
#include "halo/compiler/PerformanceData.h"
#include "llvm/ADT/StringMap.h"
#include "Logging.h"

#include <tuple>


namespace halo {

  using VertexID = CallingContextTree::VertexID;
  using Graph = CallingContextTree::Graph;


  /// This namespace provides basic graph utilities that are
  /// not provided by Boost Graph Library.
  namespace bgl {

    // does the graph have an edge from Src to Tgt?
    bool has_edge(VertexID Src, VertexID Tgt, Graph &Gr) {
      auto Range = boost::out_edges(Src, Gr);
      for (auto I = Range.first; I != Range.second; I++)
        if (boost::target(*I, Gr) == Tgt)
          return true;
      return false;
    }

    /// Search the given function's out_edge set for first target matching the given predicate,
    /// returning the matched vertex's ID
    llvm::Optional<VertexID> find_out_vertex(Graph &Gr, VertexID Src, std::function<bool(VertexInfo const&)> Pred) {
      auto Range = boost::out_edges(Src, Gr);
      for (auto I = Range.first; I != Range.second; I++) {
        auto TgtID = boost::target(*I, Gr);
        auto &TgtInfo = Gr[TgtID];
        if (Pred(TgtInfo))
          return TgtID;
      }
      return llvm::None;
    }

  }

CallingContextTree::CallingContextTree() {
  RootVertex = boost::add_vertex(VertexInfo("root"), Gr);
}

void CallingContextTree::observe(CodeRegionInfo const& CRI, PerformanceData const& PD) {
  bool SawSample = false; // FIXME: temporary

  for (pb::RawSample const& Sample : PD.getSamples()) {
    SawSample = true;
    insertSample(CRI, Sample);
  }

  if (SawSample) {
    dumpDOT(clogs());
    // fatal_error("todo: implement CCT observe");
  }
}

void CallingContextTree::insertSample(CodeRegionInfo const& CRI, pb::RawSample const& Sample) {
  // we add a sample from root downwards, so we go through the context in reverse
  // as if we are calling the sampled function.

  auto &CallChain = Sample.call_context();
  llvm::StringMap<VertexID> Ancestors; // map from name-of-vertex -> id-of-same-vertex
  auto CurrentVID = RootVertex;

  // TODO: skip / drop the first entry in the chain if it's unknown. this is the case for all call chains i've seen!

  auto IPI = CallChain.rbegin();
  auto End = CallChain.rend();

  // skip unknown functions at the base of the call chain.
  while (IPI != End && CRI.lookup(*IPI) == CRI.UnknownFI)
    IPI++;

  for (; IPI != End; IPI++) {
    uint64_t IP = *IPI;
    auto CurrentV = Gr[CurrentVID];

    // every vertex is an ancestor of itself.
    Ancestors[CurrentV.getFuncName()] = CurrentVID;

    auto FI = CRI.lookup(IP);
    auto Name = FI->getName();

    // first, check if this function is a child of CurrentVID
    auto Pred = [&](VertexInfo const& Info) { return Info.getFuncName() == Name; };
    auto MaybeChild = bgl::find_out_vertex(Gr, CurrentVID, Pred);

    VertexID Next;
    if (MaybeChild.hasValue()) {
      Next = MaybeChild.getValue();
    } else {
      // then there's currently no edge from current to a vertex with this name.

      // first, check if this is a recursive call to an ancestor.
      auto Result = Ancestors.find(Name);
      if (Result != Ancestors.end())
        Next = Result->second; // its recursive, we want a back-edge.
      else
        Next = addVertex(FI); // otherwise we make a new child

      // add the edge
      boost::add_edge(CurrentVID, Next, Gr);
    }

    CurrentVID = Next;
  }


  // auto SampledIP = Sample.instr_ptr();
  // auto SampledFI = CRI.lookup(SampledIP);

  // logs() << "====\nsampled at = "
  //         << SampledIP << "\t" << SampledFI->getName() << "\n";

  // auto CalleeID = getOrAddVertex(SampledFI);
  // // NOTE: call_context is in already in order from top to bottom of stack.

  // // I don't know why the first IP is always a junk value, so we skip it here.
  // auto Start = Sample.call_context().begin();
  // auto End = Sample.call_context().end();
  // if (Start != End && CRI.lookup(*Start) == CRI.UnknownFI)
  //   Start++;

  // assert(Start != End && SampledFI == CRI.lookup(*Start)
  //         && "first context should be the func itself!");

  // // we want the starting point to be the caller of the first context
  // Start++;

  // for (; Start != End; Start++) {
  //   uint64_t IP = *Start;
  //   auto FI = CRI.lookup(IP);

  //   // stop at the first unknown function
  //   if (FI == CRI.UnknownFI)
  //     break;

  //   logs() << IP << "\t" << FI->getName() << "\n";

  //   auto CallerID = getOrAddVertex(FI);

  //   if (!bgl::has_edge(CallerID, CalleeID, Gr))
  //     boost::add_edge(CallerID, CalleeID, Gr);

  //   CalleeID = CallerID;
  // }

  // logs() << "=====\n";
}



VertexID CallingContextTree::addVertex(FunctionInfo const* FI) {
  return boost::add_vertex(VertexInfo(FI->getName()), Gr);
}


void CallingContextTree::dumpDOT(std::ostream &out) {
  // NOTE: unfortunately we can't use boost::write_graphviz
  // found in boost/graph/graphviz.hpp because it relies on RTTI

  out << "---\ndigraph G {\n";

  auto VRange = boost::vertices(Gr);
  for (auto I = VRange.first; I != VRange.second; I++) {
    auto Vertex = *I;
    auto &Info = Gr[Vertex];

    // output vertex label
    out << Vertex
        << " [label=\""
        << Info.getDOTLabel()
        << "\"];";

    out << "\n";
  }

  // FIXME: for a better visualization, use a DFS iterator and keep track of
  // already visited vertices. then mark backedges with [style=dotted]
  auto ERange = boost::edges(Gr);
  for (auto I = ERange.first; I != ERange.second; I++) {
    auto Edge = *I;

    // output edge
    out << boost::source(Edge, Gr)
        << " -> "
        << boost::target(Edge, Gr)
        << ";\n";
  }

  out << "}\n---\n";
}


} // end namespace halo