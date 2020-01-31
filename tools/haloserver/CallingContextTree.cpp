#include "halo/compiler/CallingContextTree.h"
#include "halo/compiler/CodeRegionInfo.h"
#include "halo/compiler/PerformanceData.h"
#include "llvm/ADT/StringMap.h"
#include "Logging.h"

#include <tuple>
#include <cmath>


namespace halo {

  using VertexID = CallingContextTree::VertexID;
  using Graph = CallingContextTree::Graph;


  /// This namespace provides basic graph utilities that are
  /// not provided by Boost Graph Library.
  namespace bgl {

    // helps prevent errors with accidentially doing a vertex lookup
    // and getting a copy instead of a reference when using auto!
    VertexInfo& get(Graph &Gr, VertexID ID) {
      return Gr[ID];
    }

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
        auto &TgtInfo = get(Gr, TgtID);
        if (Pred(TgtInfo))
          return TgtID;
      }
      return llvm::None;
    }

  }

CallingContextTree::CallingContextTree() {
  RootVertex = boost::add_vertex(VertexInfo("<root>"), Gr);
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
  auto SampledIP = Sample.instr_ptr();
  auto SampledFI = CRI.lookup(SampledIP);
  bool KnownIP = SampledFI != CRI.UnknownFI;

  auto IPI = CallChain.rbegin(); // rbegin = base of call stack
  auto Top = CallChain.rend(); // rend = top of call stack

  // it is often the case that the top-most IP is a garbage IP value,
  // even when the sampled IP is in a known function.
  // I don't know what causes that, but we look for it and eliminate it here
  // so that those samples are merged into something logical.
  if (CallChain.size() > 0) {
    Top--; // move to topmost element, making it valid
    auto TopFI = CRI.lookup(*Top);
    bool KnownTop = TopFI != CRI.UnknownFI;

    if (KnownTop && TopFI == SampledFI)
      Top++; // keep the top
    else if (!KnownTop && !KnownIP)
      Top++; // it's unknown but it makes sense
    else if (KnownTop && !KnownIP)
      // it doesn't make sense if the top-most context is known but
      // we sampled at an unknown ip.
      fatal_error("inconsistent call chain top");

    // otherwise the topmost element is now
    // the end iterator, i.e., the one-past-the-end.
    // thus we drop the topmost callchain IP.
  }


  // skip unknown functions at the base of the call chain.
  while (IPI != Top && CRI.lookup(*IPI) == CRI.UnknownFI)
    IPI++;


  // now we actually process the call chain.
  llvm::StringMap<VertexID> Ancestors; // map from name-of-vertex -> id-of-same-vertex
  auto CurrentVID = RootVertex;
  for (; IPI != Top; IPI++) {
    uint64_t IP = *IPI;
    auto &CurrentV = bgl::get(Gr, CurrentVID);

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
        Next = boost::add_vertex(VertexInfo(FI), Gr); // otherwise we make a new child

      // add the edge
      boost::add_edge(CurrentVID, Next, Gr);
    }

    CurrentVID = Next;
  }

  // at this point CurrentVID is the context-sensitive function node's id
  auto &CurrentV = bgl::get(Gr, CurrentVID);
  CurrentV.observeSample(Sample);

  // TODO: add branch misprediction rate.
}


void CallingContextTree::dumpDOT(std::ostream &out) {
  // NOTE: unfortunately we can't use boost::write_graphviz
  // found in boost/graph/graphviz.hpp because it relies on RTTI

  out << "---\ndigraph G {\n";

  auto VRange = boost::vertices(Gr);
  for (auto I = VRange.first; I != VRange.second; I++) {
    auto Vertex = *I;
    auto &Info = bgl::get(Gr, Vertex);

    auto Style = Info.isPatchable() ? "solid" : "dashed";

    // output vertex and its metadata
    out << Vertex
        << " [label=\"" << Info.getDOTLabel() << "\""
        << ",style=\""  << Style << "\""
        << "];";

    out << "\n";
  }

  // FIXME: for a better visualization, use a DFS iterator and keep track of
  // already visited vertices. then mark backedges with [style=dashed]
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


/// VertexInfo definitions

void VertexInfo::observeSample(pb::RawSample const& RS) {
  const float LIMIT = 100000000.0;
  const float DISCOUNT = 0.7;
  auto ThisTime = RS.time();

  auto Diff = ThisTime - LastSampleTime;
  float Temperature = LIMIT / Diff;

  // Hotness += DISCOUNT * (Temperature - Hotness);
  Hotness += 1; // Not sure if the time thing will work unless we also track the thread id!
  LastSampleTime = ThisTime;
}

void VertexInfo::decay() {
  fatal_error("implement Decay");
}

VertexInfo::VertexInfo(FunctionInfo const* FI) :
  FuncName(FI->getName()), Patchable(FI->isPatchable()) {}

std::string VertexInfo::getDOTLabel() const {
  return FuncName + " (" + std::to_string(Hotness) + ")";
}


} // end namespace halo