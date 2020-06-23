#include "halo/compiler/CallGraph.h"
#include "halo/compiler/CallingContextTree.h"
#include "halo/compiler/CodeRegionInfo.h"
#include "halo/compiler/PerformanceData.h"
#include "halo/compiler/ReachableVisitor.h"
#include "halo/compiler/Util.h"
#include "halo/nlohmann/util.hpp"
#include "llvm/ADT/StringMap.h"
#include "Logging.h"

#include "boost/graph/depth_first_search.hpp"

#include <tuple>
#include <cmath>
#include <list>
#include <numeric>


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

  // retrieves the top-most member of the hierarchy, if one exists.
  llvm::Optional<ValTy> current() const {
    if (Sequence.size() > 0)
      return Sequence.back();
    return llvm::None;
  }

  // returns the parent of the current top-most member.
  llvm::Optional<ValTy> parent() const {
    auto Sz = Sequence.size();
    if (Sz > 1)
      return Sequence[Sz-2];
    return llvm::None;
  }

  // truncates the ancestor list to have N elements.
  void truncate(size_t N) {
    assert(N <= Sequence.size() && "why are you trying to grow it in this method?");
    Sequence.resize(N);
  }

  // shorthand for element access by index.
  auto& access(size_t Idx) { return Sequence[Idx]; }

  // returns the position in the Ancestors sequence, if an ancestor
  // name matches one of the names of the given function info.
  // the search is performed from top to back.
  llvm::Optional<size_t> findByName(std::shared_ptr<FunctionInfo> const& FI) {
    size_t Position = Sequence.size()-1;
    for (auto I = Sequence.rbegin(); I != Sequence.rend(); --Position, ++I)
      if (FI->knownAs(I->second))
        return Position;
    return llvm::None;
  }

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

    return boost::add_edge(Src, Tgt, Gr).first;
  }

  // Adds a call to the CCT starting from the Src vertex to a node equivalent to Tgt.
  // This will perform the ancestor check for recursive cases, and should be used to
  // preserve invariants of the CCT.
  //
  // There is an option to disable the ancestor check in case of a call to an unknown function.
  VertexID add_cct_call(LearningParameters const* LP,Graph &Gr, Ancestors &Ancestors, VertexID Src, std::shared_ptr<FunctionInfo> Tgt,
                        bool CheckAncestors=true) {
    // Step 1: check if the Src already has an out-edge to an equivalent Vertex.
    auto Range = boost::out_edges(Src, Gr);
    for (auto I = Range.first; I != Range.second; I++) {
      auto TgtID = boost::target(*I, Gr);
      if (Tgt->knownAs(get(Gr, TgtID).getFuncName()))
        return TgtID;
    }

    // Step 2: look in the ancestory for a node, since it might be a recursive call.
    auto Result = Ancestors.findByName(Tgt);
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
      TgtV = boost::add_vertex(VertexInfo(LP, Tgt), Gr); // otherwise we make a new child
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

CallingContextTree::CallingContextTree(LearningParameters const* lp, uint64_t samplePeriod)
: SamplePeriod(samplePeriod), LP(lp) {
  assert(LP != nullptr);
  RootVertex = boost::add_vertex(VertexInfo(LP, "<root>"), Gr);
}

void CallingContextTree::observe(CallGraph const& CG, ClientID ID, CodeRegionInfo const& CRI, PerformanceData const& PD) {
  bool SawSample = false; // FIXME: temporary

  for (pb::RawSample const& Sample : PD.getSamples()) {
    SawSample = true;
    insertSample(CG, ID, CRI, Sample);
  }

  if (SawSample)
    dumpDOT(clogs(LC_CCT_DUMP));
}

void CallingContextTree::insertSample(CallGraph const& CG, ClientID ID, CodeRegionInfo const& CRI, pb::RawSample const& Sample) {
  ///////////
  // STEP 1
  // we add a sample from root downwards, so we go through the calling-context in reverse
  // as if we are calling the sampled function.

  auto &CallChain = Sample.call_context();
  auto SampledIP = Sample.instr_ptr();
  auto SampledFI = CRI.lookup(SampledIP);
  bool KnownIP = SampledFI->isKnown();

  auto IPI = CallChain.rbegin(); // rbegin = base of call stack
  auto Top = CallChain.rend(); // rend = top of call stack

  // it is often the case that the top-most IP is a garbage IP value,
  // even when the sampled IP is in a known function.
  // I don't know what causes that, but we look for it and eliminate it here
  // so that those samples are merged into something logical.
  if (CallChain.size() > 0) {
    Top--; // move to topmost element, making it valid
    auto TopFI = CRI.lookup(*Top);
    bool KnownTop = TopFI->isKnown();

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

  // maintains current ancestors of the vertex, in order while we process the call chain.
  Ancestors Ancestors;
  auto CallerVID = RootVertex;
  auto CallerFI = CRI.getUnknown();
  std::list<VertexID> IntermediateFns;

  // now we actually process the call chain.
  while (true) {
    // every vertex is an ancestor of itself.
    auto &CallerV = bgl::get(Gr, CallerVID);
    Ancestors.push({CallerVID, CallerV.getFuncName()});

skipThisCallee:
    // our iterator is IPI
    if (IPI == Top)
      break;

    std::shared_ptr<FunctionInfo> Callee = nullptr;
    if (IntermediateFns.size() > 0) {
      // the calling context has some missing calls in the stack,
      // so we process those functions first before the IPI's function.
      auto VID = IntermediateFns.front();
      IntermediateFns.pop_front();
      Callee = CRI.lookup(bgl::get(Gr, VID).getFuncName());
    } else {
      // lookup the current entry in the calling context
      uint64_t IP = *IPI;
      Callee = CRI.lookup(IP);
      IPI++;
    }

    // FIXME: the call-graph doesn't account for FunctionInfo that has multiple
    // aliases (and new dynamic aliases)!

    bool ImaginaryCaller = CallerFI->isUnknown();
    bool ImaginaryCallee = Callee->isUnknown();
    bool DirectlyCalled = Callee->isKnown() && CG.hasCall(CallerV.getFuncName(), Callee->getCanonicalName());
    bool HasOpaqueCallee = CG.hasOpaqueCall(CallerV.getFuncName());

    logs(LC_CCT) << "Calling context contains " << CallerV.getFuncName() << " -> " << Callee->getCanonicalName()
           << "\n\tand DirectlyCalled = " << DirectlyCalled
           << ", HasOpaqueCallee = " << HasOpaqueCallee
           << ", ImaginaryCaller = " << ImaginaryCaller
           << ", ImaginaryCallee = " << ImaginaryCallee
           << "\n";

    // It's pointless to create an edge from ??? -> ???, or from root -> ???
    // so we skip the callee and remain in-place in the CCT.
    if (ImaginaryCaller && ImaginaryCallee)
      goto skipThisCallee;

    if (!ImaginaryCaller && !DirectlyCalled && !HasOpaqueCallee) {
      // Then the calling context data from perf is incorrect, because
      // it's not possible for the current function to have called the next one
      // according to the call graph.
      //
      // To avoid throwing out this data, we will check the CCT for existing paths from Current -> Callee.

      assert(IntermediateFns.size() == 0
          && "an intermediate function failed the call-graph test... CCT was bogus from the start?");

      logs(LC_CCT) << "\t\tNeed path from " << CallerV.getFuncName() << " [" << CallerVID << "] --> " << Callee->getCanonicalName() << "\n";

      auto MaybePath = shortestPath(CallerVID, Callee);
      if (!MaybePath) {
        logs(LC_CCT) << "warning: no path found. skipping sample.\n";
        return;
      }

      auto ChosenPath = MaybePath.getValue();

      logs(LC_CCT) << "\t\tUsing path: ";
      for (auto ID : ChosenPath) {
        auto &Info = bgl::get(Gr, ID);
        logs(LC_CCT) << Info.getFuncName() << " [" << ID << "]" << " -> ";
      }
      logs(LC_CCT) << "\n";

      assert(ChosenPath.size() >= 3 && "no intermediate node should have been needed?");

      // the first vertex should be the Current vertex.
      assert(bgl::get(Gr, ChosenPath.front()).getFuncName() == CallerV.getFuncName()
          && "path must start with the current vertex!");
      ChosenPath.pop_front(); // drop it

      // similarly, the last should be the callee
      assert(bgl::get(Gr, ChosenPath.back()).getFuncName() == Callee->getCanonicalName()
              && "last func in the path should be the callee that originally wasn't reachable!");
      ChosenPath.pop_back(); // drop it too

      // Now we're left with the missing functions we needed to process
      // before we continue processing the BTB.
      IntermediateFns = ChosenPath;

      assert(IntermediateFns.size() > 0);

      // now we handle the first callee in the intermediate fns, to preserve
      // the progress of the loop.
      auto VID = IntermediateFns.front();
      IntermediateFns.pop_front();
      Callee = CRI.lookup(bgl::get(Gr, VID).getFuncName());

      // un-bump the iterator since we didn't 'consume' that callee yet and went with a different one!
      IPI--;
    }

    logs(LC_CCT) << "Adding CCT call " << bgl::get(Gr, CallerVID).getFuncName() << " -> " << Callee->getCanonicalName() << "\n";
    CallerVID = bgl::add_cct_call(LP, Gr, Ancestors, CallerVID, Callee, Callee->isKnown());
    CallerFI = Callee;
  }

  // at this point CallerVID is the context-sensitive function node's id

  ////////
  // Try to find the library name

  IPI--; // go back to last IP we reached in the walk.
  auto CallerDef = CallerFI->getDefinition(*IPI);
  std::string LibraryName = CodeRegionInfo::OriginalLib;

  if (!CallerDef) {
    logs(LC_CCT) << "Unknown function definition for sampled IP in " << CallerFI->getCanonicalName() << "\n";
    goto epilogue;
  }

  LibraryName = CallerDef.getValue().Library;
  auto &CurrentV = bgl::get(Gr, CallerVID);
  CurrentV.observeSampledIP(ID, LibraryName, Sample, SamplePeriod); // add the sample to the vertex!

  logs(LC_CCT) << "Observed sample at IP in " << CallerFI->getCanonicalName() << "\n";

  ///////////
  // STEP 2
  // now we assign additional hotness by walking through the CCT step-by-step,
  // starting from the point we've identified, using the BTB
  walkBranchSamples(ID, Ancestors, CG, CallerVID, CRI, Sample);


epilogue:
#ifndef NDEBUG
  if (isMalformed()) {
    dumpDOT(clogs(LC_CCT));
    fatal_error("malformed calling-context tree!");
  }
#endif
}

/// We walk through the branch history in reverse order (recent to oldest; and to -> from) to
/// identify call edges that are frequently taken. Recall that the most recent N branches
/// contained in this history, so we can simply start at the contextually-correct starting
/// point in the CCT and walk forwards / backwards through the history.
void CallingContextTree::walkBranchSamples(ClientID ID, Ancestors &Ancestors, CallGraph const& CG,
                                           VertexID Start, CodeRegionInfo const& CRI, pb::RawSample const& Sample) {

  // Currently we assume the branch sample list may contain all sorts of branches,
  // This makes it a bit tricky bit tricky to correctly distinguish a return edge from a
  // local branch in a self-recursive function without additional information.
  // For now, in such cases we simply give up.

  dumpDOT(clogs(LC_CCT));

  logs(LC_CCT) << "walking BTB from " << CRI.lookup(Sample.instr_ptr())->getCanonicalName() << "\n";
  logs(LC_CCT) << "in the context of ancestors:\n" << Ancestors << "\n";

  // NOTE: it's helpful to think of us walking *backwards* through a history of
  // possible function transitions from most to least recent.
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

    auto From = CRI.lookup(BI.from()); // this is the function we're trying to move to.
    auto To = CRI.lookup(BI.to()); // this is the function we're current at.

    bool isCall = To->hasStart(BI.to()); // it's a call if the target is the start of the function.
    bool isRet = !isCall && To != From; // it's a return otherwise if it's in different functions.

    logs(LC_CCT) << "BTB Entry:\t" << From->getCanonicalName() << " => " << To->getCanonicalName()
           << (isCall ? "; call" :
               (isRet ? "; ret" : "; other")) << "\n";


    // Sometimes we'll get unmatched call-returns, such as the following:
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
    if (!To->knownAs(CurI.getFuncName())) {
      logs(LC_CCT) << "warning: BTB current = " << CurI.getFuncName() << "; unmatched call-return. bailing.\n-----\n";
      return;
    }

    // determine the specific library version that we're currently in.
    auto MaybeDef = To->getDefinition(BI.to());
    std::string LibraryName = CodeRegionInfo::OriginalLib;
    if (MaybeDef)
      LibraryName = MaybeDef.getValue().Library;
    else
      logs(LC_CCT) << "Unknown function definition for recently active func " << To->getCanonicalName() << "\n";


    // mark this vertex as being warm
    CurI.observeRecentlyActive(ID, LibraryName, Sample, SamplePeriod);

    // actually process this BTB entry:

    if (isCall) {
      // adding/updating the edge indicating From -called-> Cur/To, then moving UP to From.

      // searches and adjusts the ancestors to determine an appropriate "from" vertex.
      auto DetermineFromVertex = [&]() -> llvm::Optional<VertexID> {
        // We would need to do some more advanced analysis of the current Ancestors
        // to return something smarter than the below.
        //
        // A few situations where this gets tricky:
        //
        //  - Ancestors = [A, B, ???] and we're dealing with B -called-> C
        //    If the call-graph says B could call C, we could optimistically create a new
        //    C and an edge B -> C and jump over to that context.
        //
        //  - A more tricky situation is Ancestors = [A, B] and we're dealing with C -called-> D.
        //    If the call-graph says B could call C, then we could just generate C and place edges
        //    B -> C -> D.
        //    However, what if the next BTB entry is E -> C?
        //
        //
        // Instead, we just check the parent of the current vertex.
        // The Ancestors invariant is that we have [ Others ... Parent, Current]

        auto MaybeParent = Ancestors.parent();
        if (MaybeParent) {
          auto Parent = MaybeParent.getValue();
          if (From->knownAs(Parent.second)) {
            Ancestors.pop(); // move up the hierarchy
            return Parent.first;
          }
        }

        return llvm::None;
      }; // end lambda

      auto MaybeFromV = DetermineFromVertex();
      if (!MaybeFromV) {
        logs(LC_CCT) << "warning: BTB has a call from a func not encountered by the current CCT node. bailing.\n-----\n";
        return;
      }

      VertexID FromV = MaybeFromV.getValue();
      auto Edge = bgl::get_or_create_edge(FromV, Cur, Gr);

      // observe this call has having happened recently.
      auto &Info = bgl::get(Gr, Edge);
      Info.observe();
      Cur = FromV; // move to From



    } else if (isRet) {
      // the other case, Cur/To -called-> From (because this branch indicates From returned to Cur).
      // then we move to From.

      bool ImaginaryCaller = From->isUnknown();
      bool ImaginaryCallee = To->isUnknown();
      bool DirectlyCalled = From->isKnown() && CG.hasCall(CurI.getFuncName(), From->getCanonicalName());
      bool HasOpaqueCallee = CG.hasOpaqueCall(CurI.getFuncName());

      // Skip ??? -> ??? edges
      if (ImaginaryCaller && ImaginaryCallee)
        continue;

      // avoid creating 'impossible' edges in the CCT
      if (!DirectlyCalled && !HasOpaqueCallee) {
        logs(LC_CCT) << "warning: BTB claims " << CurI.getFuncName() << " called " << From->getCanonicalName()
               << " but call-graph disagrees! skipping\n----\n";
        return;
      }

      Cur = bgl::add_cct_call(LP, Gr, Ancestors, Cur, From, From->isKnown());
      Ancestors.push({Cur, From->getCanonicalName()});

    }

  }

  logs(LC_CCT) << "\nAncestors are now:\n" << Ancestors << "\n";

  dumpDOT(clogs(LC_CCT));

  logs(LC_CCT) << "---------\n";

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
  // decay vertices
  auto VRange = boost::vertices(Gr);
  for (auto I = VRange.first; I != VRange.second; I++) {
    auto &Info = bgl::get(Gr, *I);
    Info.decay();
  }
  // decay edges
  auto ERange = boost::edges(Gr);
  for (auto I = ERange.first; I != ERange.second; I++) {
    auto &Info = bgl::get(Gr, *I);
    Info.decay();
  }
}

VertexInfo CallingContextTree::getInfo(VertexID ID) const {
  return bgl::get(Gr, ID);
}

std::vector<VertexID> CallingContextTree::contextOf(VertexID Target) {

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

  // drop the root vertex from the front of the path
  Path.erase(Path.begin());

  return Path;
}


bool CallingContextTree::isMalformed() const {

  // TODO: other things we could check for:
  //
  // 1. Make sure that all children of a vertex have uniquely-named children.
  //
  // 2. Among all unique paths (ignoring back edges), make sure that no
  //    vertex appears more than once on that path, unless if it's the Unknown function.
  //
  // 3. No cross-edges or forward edges in the tree. Thus, ignoring back-edges,
  //    all nodes in tree should have exactly one parent.
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
      logs(LC_CCT) << "isMalformed: id = " << ID << "; "
             << VI.getFuncName() << " is not reachable from root!\n";
      return false;
    }
    return true; // it's reachable
  }, true);

  return !AllReachable;
}

llvm::Optional<std::list<VertexID>> CallingContextTree::shortestPath(VertexID Start, std::shared_ptr<FunctionInfo> const& Tgt) const {
  auto AllPaths = allPaths(Start, Tgt);
  if (AllPaths.empty())
    return llvm::None;

  auto AddHotness = [&](double const& Acc, VertexID const& VID) -> double {
    return Acc + bgl::get(Gr, VID).getHotness(llvm::None);
  }; // end lambda

  // find the shortest path, breaking ties by maximal hotness
  double PathHotness = std::numeric_limits<double>::min();
  size_t PathLength = std::numeric_limits<size_t>::max();
  std::list<VertexID> ChosenPath{};
  for (auto &Path : AllPaths) {
    size_t Length = Path.size();
    if (Length <= PathLength) {
      double Hotness = std::accumulate(Path.begin(), Path.end(), 0.0, AddHotness);
      if (Length < PathLength || Hotness > PathHotness) {
        PathHotness = Hotness;
        PathLength = Length;
        ChosenPath = Path;
      }
    }
  }

  assert(!ChosenPath.empty() && "path shouldn't be empty!");
  return ChosenPath;
}


std::list<std::list<VertexID>> CallingContextTree::allPaths(VertexID Start, std::shared_ptr<FunctionInfo> const& Tgt) const {
  class PathSearch : public boost::default_dfs_visitor {
  public:
    PathSearch(VertexID src, std::shared_ptr<FunctionInfo> const& tgt, std::list<std::list<VertexID>>& p) : Source(src), Target(tgt), Paths(p) {}

    // invoked on each vertex at the start of DFS-VISIT
    void discover_vertex(VertexID u, const Graph& g) {
      CurPath.push_back(u);

      auto &Info = bgl::get(g, u);
      if (CurPath.front() == Source && Target->knownAs(Info.getFuncName())) {
        // we've found a new path.
        Paths.push_back(CurPath);
      }
    }

    void finish_vertex(VertexID u, const Graph& g) {
      CurPath.pop_back();
    }

  private:
    VertexID Source;
    std::shared_ptr<FunctionInfo> const& Target;
    std::list<std::list<VertexID>>& Paths;
    std::list<VertexID> CurPath;
  }; // end class

  std::list<std::list<VertexID>> Paths;
  PathSearch Searcher(Start, Tgt, Paths);
  // We have to make a ColorMap because we want the DFS to start at a specific
  // vertex and the API is poorly designed for this case!
  auto IndexMap = boost::get(boost::vertex_index, Gr);
  auto ColorMap = boost::make_vector_property_map<boost::default_color_type>(IndexMap);
  boost::depth_first_search(Gr, Searcher, ColorMap, Start);

  return Paths;
}


///////////////////////////////
// implementation of determineIPC

struct AttrPair {
  AttrPair() : Hotness(0), IPC(0) {}
  AttrPair(CCTNodeInfo Info) : Hotness(Info.Hotness), IPC(Info.IPC) {}
  AttrPair(double heat, double ipc) : Hotness(heat), IPC(ipc) {}

  double Hotness;
  double IPC;
};

std::vector<AttrPair> toVector(CallingContextTree::Graph const& Gr,
                               std::unordered_set<CallingContextTree::VertexID> const& Group,
                               llvm::Optional<std::string> Lib) {
  std::vector<AttrPair> Vec;
  for (auto Vtex : Group) {
    auto const& Info = Gr[Vtex];
    Vec.push_back({Info.getHotness(Lib), Info.getIPC(Lib)});
  }
  return Vec;
}


/// a hotness-weighted norm function.
AttrPair euclideanNorm(std::vector<AttrPair> const& Vector) {
  // First, we need to apply a hotness-weighting to the raw IPCs,
  // so that an IPC for a function that's not being called / used
  // does not contribute to its rating in the vector

  std::vector<double> Weight;
  double TotalWeight = 0;
  for (auto const& Component : Vector) {
    double Heat = Component.Hotness;
    TotalWeight += Heat;
    Weight.push_back(Heat);
  }

  // calculate each _function_'s weight.
  if (TotalWeight != 0)
    for (unsigned i = 0; i < Weight.size(); i++)
        Weight[i] /= TotalWeight;

  int i = 0;
  AttrPair Result;
  for (auto const& Component : Vector) {
    Result.Hotness += std::pow(Component.Hotness, 2);
    Result.IPC += std::pow(Weight[i] * Component.IPC, 2);
    i++;
  }

  Result.Hotness = Result.Hotness == 0 ? 0 : std::sqrt(Result.Hotness);
  Result.IPC = Result.IPC == 0 ? 0 : std::sqrt(Result.IPC);

  return Result;
}

GroupPerf CallingContextTree::currentPerf(FunctionGroup const& FnGroup, llvm::Optional<std::string> Lib) {
  // First, we need to collect all of the starting context vertex IDs.
  // Specifically, we find all vertices in the CCT that match the root fn.
  std::vector<VertexID> RootContexts;
  {
    auto Range = boost::vertices(Gr);
    for (auto I = Range.first; I != Range.second; I++)
      if (Gr[*I].getFuncName() == FnGroup.Root)
        RootContexts.push_back(*I);
  }

  // Next, we get the vertex IDs of each function group.
  std::vector<std::unordered_set<VertexID>> Groups;
  for (auto RootID : RootContexts) {
    std::unordered_set<VertexID> &Group = Groups.emplace_back();

    // We only want to include reachable vertices that are part of the specified group.
    std::function<bool(VertexID, Graph const&)> Filter = [&FnGroup](VertexID u, Graph const& g) {
      return FnGroup.AllFuncs.count(g[u].getFuncName()) != 0;
    };

    ReachableVisitor<Graph> Visitor(Group, RootID, Filter);
    // https://www.boost.org/doc/libs/1_73_0/libs/graph/doc/bgl_named_params.html
    boost::depth_first_search(Gr, boost::visitor(Visitor).root_vertex(RootID));
  }

  // Next, we consider each _function_ within a group to be a dimension
  // in some high-dimensional space of that makes up its attributes.
  // For example, the IPC of one group is the norm of the vector that
  // is made up of the group's individual IPCs. Same goes for hotness.
  std::vector<AttrPair> AllNorms;
  size_t TotalSamples = 0;
  for (auto const& Group : Groups) {

    for (auto const& ID : Group)
      TotalSamples += Gr[ID].getSamplesSeen(Lib);

    AllNorms.push_back(euclideanNorm(toVector(Gr, Group, Lib)));
  }

  // Finally, we consider each group to be again another vector in
  // another space, and take the norm of _that_.
  // NOTE: we ignore the hotness from this final norm.
  AttrPair TotalNorm = euclideanNorm(AllNorms);

  GroupPerf Perf;
  Perf.IPC = TotalNorm.IPC;
  Perf.Hotness = TotalNorm.Hotness;
  Perf.SamplesSeen = TotalSamples;

  return Perf;
}


/////////////////////////////////



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

void VertexInfo::observeSampledIP(ClientID ID, std::string const& Lib, pb::RawSample const& RS, uint64_t Period) {
  auto TID = RS.thread_id();
  KeyType Key{ID, TID, Lib};

  observeSample(SpecificInfo[Key], RS, Period, LP->HOTNESS_SAMPLED_IP);
  observeSample(GeneralInfo, RS, Period, LP->HOTNESS_SAMPLED_IP);
}

void VertexInfo::observeRecentlyActive(ClientID ID, std::string const& Lib, pb::RawSample const& RS, uint64_t Period) {
  auto TID = RS.thread_id();
  KeyType Key{ID, TID, Lib};

  // we discount the 'hotness' of a sample at this vertex since it was only recently active, not the sampled IP
  observeSample(SpecificInfo[Key], RS, Period, LP->HOTNESS_BOOST);
  observeSample(GeneralInfo, RS, Period, LP->HOTNESS_BOOST);
}

void VertexInfo::observeSample(CCTNodeInfo &Info, pb::RawSample const& RS, uint64_t Period, float HotnessNudge) {
  // TODO: add branch mis-prediction rate?

  auto ThisTime = RS.time();

  // boost hotness by the nudge amount right away
  Info.Hotness += HotnessNudge;

  ////
  // determine how to update the IPC
  float IPCIncrement;
  bool ValidSample = true;

  if (Info.SamplesSeen == 0) {
    // the very first sample
    IPCIncrement = 0.0f;

  } else if (ThisTime >= Info.Timestamp) {
    // then this sample is ordered properly.
    // use a typical incremental update rule (Section 2.4/2.5 in RL book)
    auto ElapsedTime = ThisTime - Info.Timestamp;

    // we've seen this sample already (could be re-observing as recently active).
    // since we already boosted the hotness, we can just return.
    if (ElapsedTime == 0)
      return;

    // calculcate the movement to make for the average running IPC

    float SampleIPC = static_cast<float>(Period) / ElapsedTime; // IPC for this sample
    IPCIncrement = LP->IPC_DISCOUNT * (SampleIPC - Info.IPC);  // movement of the average

  } else {
    // otherwise this sample is out-of-order, so we skip it because
    // we expect this case to be rare!

    assert(ThisTime < Info.Timestamp && "expected out-of-order sample");
    warning("out-of-order perf sample. skipping IPC increment.\n");
    IPCIncrement = 0.0f;
    ThisTime = Info.Timestamp;
    ValidSample = false;
  }

  // actually perform the update.
  Info.SamplesSeen += (ValidSample ? 1 : 0);
  Info.IPC += IPCIncrement;
  Info.Timestamp = ThisTime;
}

CCTNodeInfo observeZeroTemp(CCTNodeInfo Info, const float DISCOUNT) {
  const float ZeroTemp = 0.0f;
  Info.Hotness += DISCOUNT * (ZeroTemp - Info.Hotness);
  if (Info.Hotness < 0.0001)
    Info.Hotness = 0.0f;
  return Info;
}

void VertexInfo::decay() {
  // take a step in the direction of reaching zero.
  // another way to think about it is that we pretend we observed
  // a zero-temperature sample
  GeneralInfo = observeZeroTemp(GeneralInfo, LP->COOLDOWN_DISCOUNT);
  for (auto& Elm : SpecificInfo)
    SpecificInfo[Elm.first] = observeZeroTemp(Elm.second, LP->COOLDOWN_DISCOUNT);
}

void VertexInfo::filterByLib(std::string const& Lib, std::function<void(CCTNodeInfo const&)> Action) const {
  for (auto const& Entry : SpecificInfo) {
    std::string EntryLib;
    std::tie(std::ignore, std::ignore, EntryLib) = Entry.first;
    if (EntryLib == Lib)
      Action(Entry.second);
  }
}


// gets a specific IPC
float VertexInfo::getIPC(llvm::Optional<std::string> Lib) const {
  if (!Lib)
    return GeneralInfo.IPC;

  std::vector<AttrPair> Obs;
  filterByLib(Lib.getValue(), [&](CCTNodeInfo const& Info){
    Obs.push_back(Info);
  });

  return euclideanNorm(Obs).IPC;
}


size_t VertexInfo::getSamplesSeen(llvm::Optional<std::string> Lib) const {
  if (!Lib)
    return GeneralInfo.SamplesSeen;

  size_t Total = 0;
  filterByLib(Lib.getValue(), [&](CCTNodeInfo const& Info){
    Total += Info.SamplesSeen;
  });
  return Total;
}

float VertexInfo::getHotness(llvm::Optional<std::string> Lib) const {
  if (!Lib)
    return GeneralInfo.Hotness;

  std::vector<AttrPair> Obs;
  filterByLib(Lib.getValue(), [&](CCTNodeInfo const& Info){
    Obs.push_back(Info);
  });

  return euclideanNorm(Obs).Hotness;
}


// FIXME: there's an arugment to be made that VertexInfo should hold onto
// the FunctionInfo pointer, since the FI info is going to be dynamic and
// the info here could become outdated.
VertexInfo::VertexInfo(LearningParameters const* lp, std::shared_ptr<FunctionInfo> FI) :
  FuncName(FI->getCanonicalName()), Patchable(FI->isPatchable()),
  LP(lp) {}

std::string VertexInfo::getDOTLabel() const {
  return FuncName + " (hot=" + to_formatted_str(getHotness(llvm::None)) + ";ipc=" + to_formatted_str(getIPC(llvm::None)) + ")";
}


/////////////
// EdgeInfo implementations

// (0, 1), learning rate / incremental update factor "alpha"
const float EdgeInfo::FREQUENCY_DISCOUNT = 0.7f;

void EdgeInfo::observe() {
  Frequency += 1.0f;
}

void EdgeInfo::decay() {
  // take a step in the direction of reaching zero.
  const float ZeroTemp = 0.0f;
  Frequency += FREQUENCY_DISCOUNT * (ZeroTemp - Frequency);
}


////////////////
// LearningParameters

LearningParameters::LearningParameters(nlohmann::json const& Config)
  : IPC_DISCOUNT(config::getServerSetting<float>("cct-ipc-discount", Config))
  , COOLDOWN_DISCOUNT(config::getServerSetting<float>("cct-cooldown-discount", Config))
  , HOTNESS_SAMPLED_IP(config::getServerSetting<float>("cct-hotness-ipsample", Config))
  , HOTNESS_BOOST(config::getServerSetting<float>("cct-hotness-recentlyactive", Config))
  {
    assert(0 < IPC_DISCOUNT && IPC_DISCOUNT <= 1.0f);
    assert(0 < COOLDOWN_DISCOUNT && COOLDOWN_DISCOUNT <= 1.0f);
  }

} // end namespace halo