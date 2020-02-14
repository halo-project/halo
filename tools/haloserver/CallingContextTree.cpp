#include "halo/compiler/CallingContextTree.h"
#include "halo/compiler/CodeRegionInfo.h"
#include "halo/compiler/PerformanceData.h"
#include "halo/compiler/Util.h"
#include "llvm/ADT/StringMap.h"
#include "Logging.h"

#include "boost/graph/depth_first_search.hpp"

#include <tuple>
#include <cmath>


namespace halo {

  using VertexID = CallingContextTree::VertexID;
  using EdgeID = CallingContextTree::EdgeID;
  using Graph = CallingContextTree::Graph;


  /// This namespace provides basic graph utilities that are
  /// not provided by Boost Graph Library.
  namespace bgl {

    // helps prevent errors with accidentially doing a vertex lookup
    // and getting a copy instead of a reference when using auto!
    inline VertexInfo& get(Graph &Gr, VertexID ID) {
      return Gr[ID];
    }

    inline EdgeInfo& get(Graph &Gr, EdgeID Edge) {
      return Gr[Edge];
    }

    // does the graph have an edge from Src --> Tgt? if so, return it.
    llvm::Optional<EdgeID> get_edge(VertexID Src, VertexID Tgt, Graph &Gr) {
      auto Range = boost::out_edges(Src, Gr);
      for (auto I = Range.first; I != Range.second; I++)
        if (boost::target(*I, Gr) == Tgt)
          return *I;
      return llvm::None;
    }

    /// Search the given vertex's OUT_EDGE set for first edge who's TARGET matches the given predicate,
    /// returning the matched vertex's ID
    llvm::Optional<VertexID> find_out_vertex(Graph &Gr, VertexID Vtex, std::function<bool(VertexInfo const&)> Pred) {
      auto Range = boost::out_edges(Vtex, Gr);
      for (auto I = Range.first; I != Range.second; I++) {
        auto TgtID = boost::target(*I, Gr);
        auto &TgtInfo = get(Gr, TgtID);
        if (Pred(TgtInfo))
          return TgtID;
      }
      return llvm::None;
    }

    /// Search the given vertex's IN_EDGE set for first edge who's SOURCE matches the given predicate,
    /// returning the matched vertex's ID
    llvm::Optional<VertexID> find_in_vertex(Graph &Gr, VertexID Vtex, std::function<bool(VertexInfo const&)> Pred) {
      auto Range = boost::in_edges(Vtex, Gr);
      for (auto I = Range.first; I != Range.second; I++) {
        auto TgtID = boost::source(*I, Gr);
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

void CallingContextTree::observe(ClientID ID, CodeRegionInfo const& CRI, PerformanceData const& PD) {
  bool SawSample = false; // FIXME: temporary

  for (pb::RawSample const& Sample : PD.getSamples()) {
    SawSample = true;
    insertSample(ID, CRI, Sample);
  }

  if (SawSample) {
    dumpDOT(clogs());
    // fatal_error("todo: implement CCT observe");
  }
}

void CallingContextTree::insertSample(ClientID ID, CodeRegionInfo const& CRI, pb::RawSample const& Sample) {
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
  CurrentV.observeSample(ID, Sample); // add the sample to the vertex!

  dumpDOT(clogs());

  walkBranchSamples(CurrentVID, CRI, Sample);
}

/// We walk through the branch history in reverse order (recent to oldest) to identify
/// call edges that are frequently taken. Recall that both calls and returns are contained
/// in this history, so we can simply start at the contextually-correct starting point
/// in the CCT and walk forwards / backwards through the history.
void CallingContextTree::walkBranchSamples(VertexID Start, CodeRegionInfo const& CRI, pb::RawSample const& Sample) {

  // Currently we assume the branch sample list only contains call / return branches,
  // not conditional ones etc. The reason is that I think it might be a bit tricky to
  // correctly distinguish a return edge from a local branch in a self-recursive function
  // without additional information.

  VertexID Cur = Start;
  for (auto &BI : Sample.branch()) {
    auto &CurI = bgl::get(Gr, Cur);
    auto From = CRI.lookup(BI.from());
    auto To = CRI.lookup(BI.to());

    auto BoostFrequency = [&](VertexID Src, VertexID Tgt) -> void {
      auto MaybeEdge = bgl::get_edge(Src, Tgt, Gr);
      if (!MaybeEdge)
        fatal_error("expected an edge here!");

      auto &EdgeInfo = bgl::get(Gr, MaybeEdge.getValue());
      EdgeInfo.observe();
    };

    logs() << "BTB " << From->getName() << " => " << To->getName() << "\n";

    continue;

    if (CurI.getFuncName() == To->getName()) {
      // we're looking for an edge ?? -> Current, so a call to Current happened.
      auto Pred = [&](VertexInfo const& Info){ return Info.getFuncName() == From->getName(); };
      auto MaybeV = bgl::find_in_vertex(Gr, Cur, Pred);

      if (!MaybeV) {
        logs() << "call warning: edge from " << From->getName() << " -> " << CurI.getFuncName() << " doesn't exist!\n";
        fatal_error("CALL stopping here for now");
        continue;
      }

      auto FromVertex = MaybeV.getValue();
      BoostFrequency(FromVertex, Cur);
      Cur = FromVertex; // advance

    } else if (CurI.getFuncName() == From->getName()) {
      // we're looking for an edge Current -> ??, so a return from Current happened.
      auto Pred = [&](VertexInfo const& Info){ return Info.getFuncName() == To->getName(); };
      auto MaybeV = bgl::find_out_vertex(Gr, Cur, Pred);

      if (!MaybeV) {
        logs() << "ret warning: edge from " << CurI.getFuncName() << " -> " << To->getName() << " doesn't exist!\n";
        fatal_error("RET stopping here for now");
        continue;
      }

      auto ToVertex = MaybeV.getValue();
      BoostFrequency(Cur, ToVertex);
      Cur = ToVertex; // advance

    } else {
      logs() << "neither warning: edge from " << From->getName() << " -> " << To->getName() << " doesn't exist\n";
    }
  }

  logs() << "---------\n";

}


template <typename AccTy>
AccTy CallingContextTree::reduce(std::function<AccTy(VertexID, VertexInfo const&, AccTy)> F, AccTy Initial) {
  AccTy Result = Initial;
  auto VRange = boost::vertices(Gr);
  for (auto I = VRange.first; I != VRange.second; I++)
    Result = F(*I, bgl::get(Gr, *I), Result);

  return Result;
}

// instances of reduce. NO ANGLE BRACKETS!
template VertexID CallingContextTree::reduce(std::function<VertexID(VertexID, VertexInfo const&, VertexID)> F, VertexID Initial);


void CallingContextTree::decay() {
  auto VRange = boost::vertices(Gr);
  for (auto I = VRange.first; I != VRange.second; I++) {
    auto &Info = bgl::get(Gr, *I);
    Info.decay();
  }
}


std::vector<VertexInfo> CallingContextTree::contextOf(VertexID Target) {

  class ContextSearch : public boost::default_dfs_visitor {
  public:
    ContextSearch(VertexID end, std::vector<VertexID>& p) : End(end), Path(p) {}

    // invoked when the vertex is encountered the first time in the DFS.
    // this is what we want to avoid cycles!
    void discover_vertex(VertexID u, const Graph& g) {
      if (Found)
        return;

      Path.push_back(u);

      Found = (u == End);
    }

    void finish_vertex(VertexID u, const Graph& g) {
      if (Found)
        return;

      Path.pop_back();
    }

  private:
    VertexID End;
    bool Found = false;
    std::vector<VertexID>& Path;
  }; // end class

  std::vector<VertexID> Path;
  ContextSearch Searcher(Target, Path);
  boost::depth_first_search(Gr, boost::visitor(Searcher));

  if (Path.size() < 1 || Path[0] != RootVertex)
    fatal_error("empty path or one that does not originate from root!");

  if (Path[Path.size()-1] != Target)
    fatal_error("path does not end at the target!");

  std::vector<VertexInfo> VI;
  // add all but the root vertex to the returned info.
  for (auto I = Path.begin()+1; I != Path.end(); I++)
    VI.push_back(bgl::get(Gr, *I));

  return VI;
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
    auto &Info = bgl::get(Gr, Edge);

    // output edge
    out << boost::source(Edge, Gr)
        << " -> "
        << boost::target(Edge, Gr)
        << " [label=\"" << to_formatted_str(Info.getFrequency())
        << "\"];\n";
  }

  out << "}\n---\n";
}

///////////////////////////////////////////////
/// VertexInfo definitions

const float VertexInfo::HOTNESS_BASELINE = 100000000.0f;
const float VertexInfo::HOTNESS_DISCOUNT = 0.7f;

void VertexInfo::observeSample(ClientID ID, pb::RawSample const& RS) {
  // TODO: add branch mis-prediction rate?

  auto ThisTime = RS.time();
  auto TID = RS.thread_id();

  std::pair<ClientID, uint32_t> Key{ID, TID};
  auto LastTime = LastSampleTime[Key];

  if (LastTime) {
    // use a typical incremental update rule (Section 2.4/2.5 in RL book)
    auto Diff = ThisTime - LastTime;
    float Temperature = HOTNESS_BASELINE / Diff;
    Hotness += HOTNESS_DISCOUNT * (Temperature - Hotness);
  }

  LastSampleTime[Key] = ThisTime;
}

void VertexInfo::decay() {
  // take a step in the direction of reaching zero.
  // another way to think about it is that we pretend we observed
  // a zero-temperature sample
  const float ZeroTemp = 0.0f;
  Hotness += HOTNESS_DISCOUNT * (ZeroTemp - Hotness);
}

VertexInfo::VertexInfo(FunctionInfo const* FI) :
  FuncName(FI->getName()), Patchable(FI->isPatchable()) {}

std::string VertexInfo::getDOTLabel() const {
  return FuncName + " (" + to_formatted_str(Hotness) + ")";
}


/////////////
// EdgeInfo implementations

void EdgeInfo::observe() {
  Frequency += 1.0f;
}


} // end namespace halo