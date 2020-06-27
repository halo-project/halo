#include "halo/compiler/Profiler.h"
#include "halo/server/ClientSession.h"
#include "halo/server/ClientGroup.h"
#include "halo/nlohmann/util.hpp"

#include "Messages.pb.h"
#include <algorithm>

namespace halo {

Profiler::Profiler(JSON const& Config)
  : SamplePeriod(config::getServerSetting<uint64_t>("perf-sample-period", Config))
  , LP(Config)
  , CCT(&LP, SamplePeriod)
  {}

void Profiler::consumePerfData(GroupState &State) {
  for (auto &CS : State.Clients) {
    auto &State = CS->State;
    auto &Samples = State.PerfData.getSamples();
    SamplesSeen += Samples.size();

    // Perform a sorting operation over timestamps so they're correctly
    // ordered to compute IPCs.
    // Of course, between batches there could be an out-of-order samples,
    // but sorting should fix mis-orderings in nearly every case.
    std::sort(Samples.begin(), Samples.end(),
      // less-than comparator
      [](pb::RawSample const& A, pb::RawSample const& B) {
        return A.time() < B.time();
    });

    CCT.observe(CG, State.ID, State.CRI, State.PerfData);
    State.PerfData.clear();
  }

  // age the data
  decay();
}

void Profiler::decay() {
  CCT.decay();
}

GroupPerf Profiler::currentPerf(FunctionGroup const& FnGroup, llvm::Optional<std::string> LibName) {
  return CCT.currentPerf(FnGroup, LibName);
}

llvm::Optional<Profiler::CCTNode> Profiler::hottestNode() {
  // find the hottest VID
  using VertexID = CCTNode;
  VertexID Root = CCT.getRoot();
  float Max = 0.0f;
  VertexID MaxVID = CCT.reduce<VertexID>([&](VertexID ID, VertexInfo const& VI, VertexID AccID)  {
    auto Hotness = VI.getHotness(llvm::None);
    if (Hotness > Max) {
      Max = Hotness;
      return ID;
    }
    return AccID;
  }, Root);

  if (MaxVID == Root)
    return llvm::None;

  return MaxVID;
}


  // Walk backwards towards 'root' and look first for the very-first first patchable function
  // that can reach the given vertex.
  //
  // Then we consider expanding the scope from current to the candidate if both candidate
  // and parent both hot.
  //
  //   (parent : hot)  --A--> (candidate : hot) --B--> { (current : hot) --> ... }
  //
  // The reasoning here is that if the parent is cold, then there may not be enough
  // call-return happening on edge A for the candidate to be a suitable tuning root, since code changes
  // (and thus tuning progress) only happens on calls to the tuning root.
  //
llvm::Optional<std::string> Profiler::findSuitableTuningRoot(Profiler::CCTNode HotVID) {
  const double MINIMUM_ROOT_HOTNESS = 2.0;
  const double MINIMUM_PARENT_HOTNESS = MINIMUM_ROOT_HOTNESS / 4;
  llvm::Optional<std::string> Suitable;
  llvm::Optional<CCTNode> CandidateID;

  auto SuitableTuningRoot = [&](VertexInfo const& VI) {
    bool Patchable = VI.isPatchable();
    float Hotness = VI.getHotness(llvm::None);
    clogs() << "Patchable = " << Patchable << ", Hotness = " << Hotness << "\n";
    return Patchable && Hotness >= MINIMUM_ROOT_HOTNESS;
  };

  CCT.dumpDOT(clogs());

  // walk the context of this hot node. the context always includes the node itself at the start.
  auto CxtIDs = CCT.contextOf(HotVID);
  assert(*(CxtIDs.rbegin()) == HotVID);

  for (auto IDI = CxtIDs.rbegin(); IDI != CxtIDs.rend(); IDI++) {
    auto const& Parent = CCT.getInfo(*IDI);

    // is there no candidate?
    if (!CandidateID.hasValue()) {
      // check if this one is okay
      if (SuitableTuningRoot(Parent)) {
        CandidateID = *IDI;
      }
       continue; // keep looking
    }

    CCTNode CandID = CandidateID.getValue();
    auto const& Candidate = CCT.getInfo(CandID);

    float ParentHotness = Parent.getHotness(llvm::None);
    float CandidateCalledFreq = CCT.getCallFrequency(CandID);
    clogs() << "Parent Hotness = " << ParentHotness << ", CandidateCalledFreq = " << CandidateCalledFreq <<  "\n";

    // the candidate is suitable if its parent is either hot or has called the candidate recently
    if (ParentHotness >= MINIMUM_PARENT_HOTNESS || CandidateCalledFreq > 0) {
      Suitable = Candidate.getFuncName();

      // could the parent also be a possible tuning root?
      if (SuitableTuningRoot(Parent)) {
        CandidateID = *IDI;
        continue;
      }
    }

    break; // stop!
  }

  return Suitable;
}

void Profiler::dump(llvm::raw_ostream &out) {
  fatal_error("implement Profiler::dump!");
}

} // end namespace halo