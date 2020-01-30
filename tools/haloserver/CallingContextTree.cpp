#include "halo/compiler/CallingContextTree.h"
#include "halo/compiler/CodeRegionInfo.h"
#include "halo/compiler/PerformanceData.h"
#include "Logging.h"

#include <tuple>


namespace halo {

  using VertexID = CallingContextTree::VertexID;
  using Graph = CallingContextTree::Graph;


  /// This namespace provides basic graph utilities that are
  /// not provided by Boost Graph Library.
  namespace bgl {
    bool has_edge(VertexID Src, VertexID Tgt, Graph &Gr) {
      auto Range = boost::out_edges(Src, Gr);
      for (auto I = Range.first; I != Range.second; I++)
        if (boost::target(*I, Gr) == Tgt)
          return true;
      return false;
    }
  }

void CallingContextTree::observe(CodeRegionInfo const& CRI, PerformanceData const& PD) {
  bool SawSample = false; // FIXME: temporary

  for (pb::RawSample const& Sample : PD.getSamples()) {
    SawSample = true;

    auto SampledIP = Sample.instr_ptr();
    auto SampledFI = CRI.lookup(SampledIP);

    logs() << "====\nsampled at = "
           << SampledIP << "\t" << SampledFI->getName() << "\n";

    auto CalleeID = getOrAddVertex(SampledFI);
    // NOTE: call_context is in already in order from top to bottom of stack.

    // I don't know why the first IP is always a junk value, so we skip it here.
    auto Start = Sample.call_context().begin();
    auto End = Sample.call_context().end();
    if (Start != End && CRI.lookup(*Start) == CRI.UnknownFI)
      Start++;

    assert(Start != End && SampledFI == CRI.lookup(*Start)
            && "first context should be the func itself!");

    // we want the starting point to be the caller of the first context
    Start++;

    for (; Start != End; Start++) {
      uint64_t IP = *Start;
      auto FI = CRI.lookup(IP);

      // stop at the first unknown function
      if (FI == CRI.UnknownFI)
        break;

      logs() << IP << "\t" << FI->getName() << "\n";

      auto CallerID = getOrAddVertex(FI);

      if (!bgl::has_edge(CallerID, CalleeID, Gr))
        boost::add_edge(CallerID, CalleeID, Gr);

      CalleeID = CallerID;
    }

    logs() << "=====\n";
  }

  if (SawSample) {
    dumpDOT(clogs());
    // fatal_error("todo: implement CCT observe");
  }
}


CallingContextTree::VertexID CallingContextTree::getOrAddVertex(FunctionInfo const* FI) {
  auto Result = lookup(FI->getName());

  if (!Result.hasValue()) {
    auto Name = FI->getName();
    VertexInfo VI(Name);
    auto VID = boost::add_vertex(VI, Gr);
    VertexMap[Name] = VID;
    return VID;
  }

  return Result.getValue();
}

llvm::Optional<CallingContextTree::VertexID> CallingContextTree::lookup(std::string const& VName) {
  auto Result = VertexMap.find(VName);
  if (Result != VertexMap.end())
    return Result->second;

  return llvm::None;
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