#include "halo/compiler/Profiler.h"

#include "halo/server/ClientSession.h"
#include "Messages.pb.h"

namespace halo {

void Profiler::consumePerfData(ClientList & Clients) {
  for (auto &CS : Clients) {
    auto &State = CS->State;
    SamplesSeen += State.PerfData.getSamples().size();
    CCT.observe(CG, State.ID, State.CRI, State.PerfData);
    State.PerfData.clear();
  }
}

void Profiler::decay() {
  CCT.decay();
}

double Profiler::determineIPC(FunctionGroup const& FnGroup) {
  return CCT.determineIPC(FnGroup);
}

llvm::Optional<Profiler::CCTNode> Profiler::hottestNode() {
  // find the hottest VID
  using VertexID = CCTNode;
  VertexID Root = CCT.getRoot();
  float Max = 0.0f;
  VertexID MaxVID = CCT.reduce<VertexID>([&](VertexID ID, VertexInfo const& VI, VertexID AccID)  {
    auto Hotness = VI.getHotness();
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

llvm::Optional<std::string> Profiler::findSuitableTuningRoot(Profiler::CCTNode MaxVID) {
  // obtain the context of that VID
  auto CxtIDs = CCT.contextOf(MaxVID);

  // Walk backwards towards 'main' and look first for the very-first first patchable function
  // that can reach the given vertex.
  //
  // Then we consider expanding the scope of the tuning section if the next parent
  // both patchable & hot enough.
  //
  // TODO: For now we only expand the scope one level, to the parent. We should possibly expand further??
  //
  // The reasoning here is that if a parent is cold but its child is hot,
  // then it must not be calling the child often enough to be a suitable tuning root.
  // Otherwise, if the parent is hot then there must be some call-return happening
  // and we can expand the scope.

  const double MINIMUM_HOTNESS = 2.0;
  llvm::Optional<std::string> Suitable = llvm::None;
  for (auto IDI = CxtIDs.rbegin(); IDI != CxtIDs.rend(); IDI++) {
    auto VI = CCT.getInfo(*IDI);
    if (!Suitable.hasValue() && VI.isPatchable()) {
      Suitable = VI.getFuncName();
    } else {
      // an ancestor of the first suitable.
      if (VI.getHotness() >= MINIMUM_HOTNESS)
        return VI.getFuncName();
      return Suitable;
    }
  }

  return Suitable;
}

void Profiler::setBitcodeStatus(std::string const& Name, bool Status) {
  if (Status)
    FuncsWithBitcode.insert(Name);
  else
    FuncsWithBitcode.erase(Name);
}

void Profiler::dump(llvm::raw_ostream &out) {
  fatal_error("implement Profiler::dump!");
}

} // end namespace halo