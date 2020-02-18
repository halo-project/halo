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


// maintains a stack of ancestors for use during CCT modification operations.
class Ancestors {
public:
  using ValTy = std::pair<VertexID, std::string>;
  using Type = std::vector<ValTy>;

  // pushes a new ancestor onto the stack
  void push(ValTy &&V) {
    Sequence.push_back(V);
  }

  // blindly removes and returns the top-most ancestor from the stack.
  // this shrinks the ancestor sequence by 1.
  ValTy pop() {
    auto V = Sequence.back();
    Sequence.pop_back();
    return V;
  }

  // removes and returns the top-most ancestor if its name matches the given name.
  llvm::Optional<ValTy> pop_if_same(std::string const& Name) {
    auto V = Sequence.back();
    if (V.second == Name) {
      Sequence.pop_back();
      return V;
    }
    return llvm::None;
  }

  // truncates the ancestor list to have N elements.
  void truncate(size_t N) {
    assert(N <= Sequence.size() && "why are you trying to grow it like this?");
    Sequence.resize(N);
  }

  // shorthand for element access by index.
  auto& access(size_t Idx) { return Sequence[Idx]; }

  // returns the position in the Ancestors sequence, if found.
  llvm::Optional<size_t> findByName(std::string const& Name) {
      size_t Position = Sequence.size()-1;
      for (auto I = Sequence.rbegin(); I != Sequence.rend(); --Position, ++I)
        if (I->second == Name)
          return Position;
      return llvm::None;
  };

  friend llvm::raw_ostream& operator<<(llvm::raw_ostream& os, Ancestors const&);

private:
    Type Sequence;

}; // end class Ancestors

llvm::raw_ostream& operator<<(llvm::raw_ostream& os, Ancestors const& Anscestors) {
  for (auto const& P : Anscestors.Sequence)
    os << "id = " << P.first << "; " << P.second << "\n";
  return os;
}



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

  inline VertexInfo const& get(Graph const& Gr, VertexID ID) {
    return Gr[ID];
  }

  inline EdgeInfo const& get(Graph const& Gr, EdgeID Edge) {
    return Gr[Edge];
  }

  // does the graph have an edge from Src --> Tgt? if so, return it.
  llvm::Optional<EdgeID> get_edge(VertexID Src, VertexID Tgt, Graph &Gr) {
    auto OutRange = boost::out_edges(Src, Gr);
    for (auto I = OutRange.first; I != OutRange.second; I++)
      if (boost::target(*I, Gr) == Tgt)
        return *I;

    // FIXME: is this check actually redundant?
    auto InRange = boost::in_edges(Tgt, Gr);
    for (auto I = InRange.first; I != InRange.second; I++)
      if (boost::source(*I, Gr) == Src)
        return *I;

    return llvm::None;
  }

  EdgeID get_or_create_edge(VertexID Src, VertexID Tgt, Graph &Gr) {
    auto MaybeE = get_edge(Src, Tgt, Gr);
    if (MaybeE)
      return MaybeE.getValue();

    boost::add_edge(Src, Tgt, Gr);

    return get_or_create_edge(Src, Tgt, Gr); // should never iterate more than once.
  }

  // Adds a call to the CCT starting from the Src vertex to a node equivalent to Tgt.
  // This will perform the ancestor check for recursive cases, and should be used to
  // preserve invariants of the CCT.
  //
  // There is an option to disable the ancestor check in case of a call to an unknown function.
  VertexID add_cct_call(Graph &Gr, Ancestors &Ancestors, VertexID Src, FunctionInfo *Tgt,
                        bool CheckAncestors=true) {
    // Step 1: check if the Src already has an out-edge to an equivalent Vertex.
    auto Range = boost::out_edges(Src, Gr);
    auto &Name = Tgt->getName();
    for (auto I = Range.first; I != Range.second; I++) {
      auto TgtID = boost::target(*I, Gr);
      if (get(Gr, TgtID).getFuncName() == Name)
        return TgtID;
    }

    // Step 2: look in the ancestory for a node, since it might be a recursive call.
    auto Result = Ancestors.findByName(Name);
    VertexID TgtV;
    if (CheckAncestors && Result) {
      // we're going to make a back-edge to the ancestor
      size_t Idx = Result.getValue();
      TgtV = Ancestors.access(Idx).first;

      // since we're moving back up the calling-context, we cut the ancestors sequence down.
      // if B is the recursive ancestor in [root ... A, B, ... ]
      // then we cut it down to [root ... A]
      Ancestors.truncate(Idx);
    } else {
      // it's neither an ancestor nor a child, so we make a new child.
      TgtV = boost::add_vertex(VertexInfo(Tgt), Gr); // otherwise we make a new child
    }

    // finally, make the edge.
    boost::add_edge(Src, TgtV, Gr);
    return TgtV;
  }

  /// Search the given vertex's IN_EDGE set for first edge who's SOURCE matches the given predicate,
  /// returning the matched vertex's ID
  llvm::Optional<VertexID> find_in_vertex(Graph &Gr, VertexID Vtex, std::function<bool(VertexInfo const&)> Pred) {
    auto Range = boost::in_edges(Vtex, Gr);
    for (auto I = Range.first; I != Range.second; I++) {
      auto TgtID = boost::source(*I, Gr);
      if (Pred(get(Gr, TgtID)))
        return TgtID;
    }
    return llvm::None;
  }

} // end namespace bgl



//////
// CallingContextTree definitions

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


  // maintains current ancestors of the vertex, in order
  Ancestors Ancestors;
  auto CurrentVID = RootVertex;
  // now we actually process the call chain.
  while (true) {
    // every vertex is an ancestor of itself.
    auto &CurrentV = bgl::get(Gr, CurrentVID);
    Ancestors.push({CurrentVID, CurrentV.getFuncName()});

    // our iterator is IPI
    if (IPI == Top)
      break;

    uint64_t IP = *IPI;
    auto FI = CRI.lookup(IP);
    CurrentVID = bgl::add_cct_call(Gr, Ancestors, CurrentVID, FI, FI != CRI.UnknownFI);
    IPI++;
  }

  // at this point CurrentVID is the context-sensitive function node's id
  auto &CurrentV = bgl::get(Gr, CurrentVID);
  CurrentV.observeSample(ID, Sample); // add the sample to the vertex!

  walkBranchSamples(Ancestors, CurrentVID, CRI, Sample);

  // TODO: maybe only if NDEBUG ?
  if (isMalformed()) {
    dumpDOT(clogs());
    fatal_error("malformed calling-context tree!");
  }
}

/// We walk through the branch history in reverse order (recent to oldest & to -> from) to
/// identify call edges that are frequently taken. Recall that both calls and returns are
/// contained in this history, so we can simply start at the contextually-correct starting
/// point in the CCT and walk forwards / backwards through the history.
void CallingContextTree::walkBranchSamples(Ancestors &Ancestors, VertexID Start, CodeRegionInfo const& CRI, pb::RawSample const& Sample) {

  // Currently we assume the branch sample list only contains call / return branches,
  // not conditional ones etc. The reason is that I think it might be a bit tricky to
  // correctly distinguish a return edge from a local branch in a self-recursive function
  // without additional information.

  dumpDOT(clogs());

  logs() << "walking BTB from " << CRI.lookup(Sample.instr_ptr())->getName() << "\n";
  logs() << "in the context of ancestors:\n" << Ancestors << "\n";

  // NOTE: it's helpful to think of us walking *backwards* through a history of
  // function transitions from most to least recent.
  //
  // Thus, the 'To' part of each individual branch is where we should already be,
  // and we always move *back* towards the 'From' before inspecting the next branch:
  //
  // B => A; call.
  // |
  // +----+
  //      |
  //      v
  // C => B; ret.
  // |
  // +----+
  //      |
  //      v
  // D => C; ret.
  // +----+
  //      |
  //      v  etc...
  // E => D; ret.
  // D => E; call.
  // C => D; call.

  VertexID Cur = Start;
  for (auto &BI : Sample.branch()) {
    auto &CurI = bgl::get(Gr, Cur);

    // this is the vertex we're trying to move to.
    auto From = CRI.lookup(BI.from());
    auto &FromName = From->getName();

    // predicate function that returns true if the info matches the FromName of this branch.
    auto Chooser = [&](VertexInfo const& Info) { return Info.getFuncName() == FromName; };

    auto To = CRI.lookup(BI.to());

    // it's a call if the target is the start of the function.
    bool isCall = To->getStart() == BI.to();

    logs() << "BTB Entry:\t" << FromName << " => " << To->getName()
           << (isCall ? "; call" : "; ret") << "\n";


    // FIXME: sometimes we'll get unmatched call-returns, such as the following:
    //
    //  ??? => A; ret
    //  B   => A; call
    //
    // I don't know how this happens. It might be some imprecision in the CPU's performance
    // counters, or a short-jump that isn't recorded by Perf but performs a function call.
    // The latter might be caused by our own XRay instrumentation, so it's something
    // to watch out for in the future.
    //
    // TODO: just assume that a call A => ??? happened. So long as the target is the
    // unknown function it shouldn't mess anything up and will record what's going on
    // more accurately.
    if (To->getName() != CurI.getFuncName()) {
      logs() << "warning: current = " << CurI.getFuncName() << ", bailing.\n-----\n";
      return;
    }

    if (To == CRI.UnknownFI && From == CRI.UnknownFI)
      continue;

    if (isCall) {
      // adding/updating the edge indicating From -called-> Cur, then moving UP to From.
      VertexID FromV;

      // FIXME: this makes weird tree knots and outgrowths sometimes.
      auto Result = bgl::find_in_vertex(Gr, Cur, Chooser);
      if (Result)
        FromV = Result.getValue();
      else
        FromV = boost::add_vertex(VertexInfo(From), Gr);

      auto Edge = bgl::get_or_create_edge(FromV, Cur, Gr);

      // observe this call has having happened recently.
      auto &Info = bgl::get(Gr, Edge);
      Info.observe();

      Ancestors.pop_if_same(CurI.getFuncName()); // TODO: shouldn't we make an edge or soemthing?
      Cur = FromV;

    } else {
      // the other case, Cur -called-> From (because this branch indicates From returned to Cur).
      // then we move to From.

      Cur = bgl::add_cct_call(Gr, Ancestors, Cur, From, From != CRI.UnknownFI);
      Ancestors.push({Cur, FromName});
    }

  }

  logs() << "---------\n";

}


template <typename AccTy>
AccTy CallingContextTree::reduce(std::function<AccTy(VertexID, VertexInfo const&, AccTy)> F, AccTy Initial) const {
  AccTy Result = Initial;
  auto VRange = boost::vertices(Gr);
  for (auto I = VRange.first; I != VRange.second; I++)
    Result = F(*I, bgl::get(Gr, *I), Result);

  return Result;
}

// instances of reduce. NO ANGLE BRACKETS!
template VertexID CallingContextTree::reduce(std::function<VertexID(VertexID, VertexInfo const&, VertexID)> F, VertexID Initial) const;
template bool CallingContextTree::reduce(std::function<bool(VertexID, VertexInfo const&, bool)> F, bool Initial) const;


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


bool CallingContextTree::isMalformed() const {

  // TODO: other things we could check for:
  //
  // 1. Make sure that all children of a vertex have uniquely-named children.
  //
  // 2. Among all unique paths (ignoring back edges), make sure that no
  //    vertex appears more than once on that path, unless if it's the Unknown function.
  //
  // 3. No cross-edges or forward edges in the tree.
  //

  // Goals are to collect information about
  //
  // 1. Vertices that are reachable from the Root.
  //
  class CorrectnessVisitor : public boost::default_dfs_visitor {
  public:
    CorrectnessVisitor(std::unordered_set<VertexID>& visited, VertexID root)
                                                : Root(root), Visited(visited) {}

    void discover_vertex(VertexID u, const Graph& g) {
      Stack.push_back(u);

      if (Stack.front() == Root)
        Visited.insert(u);
    }

    void finish_vertex(VertexID u, const Graph& g) {
      Stack.pop_back();
    }

  private:
    VertexID Root;
    std::unordered_set<VertexID>& Visited;
    std::vector<VertexID> Stack; // internal bookkeeping
  }; // end class

  std::unordered_set<VertexID> ReachableFromRoot;
  CorrectnessVisitor CV(ReachableFromRoot, RootVertex);
  boost::depth_first_search(Gr, boost::visitor(CV));

  // Check that all vertices are reachable from the root
  bool AllReachable = reduce<bool>([&](VertexID ID, VertexInfo const& VI, bool Acc) -> bool {
    if (!Acc)
      return Acc;

    auto Result = ReachableFromRoot.find(ID);
    if (Result == ReachableFromRoot.end()) {
      logs() << "isMalformed: id = " << ID << "; " << VI.getFuncName() << " is not reachable from root!\n";
      return false;
    }
    return true; // it's reachable
  }, true);

  return !AllReachable;
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