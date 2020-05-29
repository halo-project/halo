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

llvm::Optional<std::string> Profiler::getFirstPatchableInContext(Profiler::CCTNode MaxVID) {
  // obtain the context of that VID
  auto CxtIDs = CCT.contextOf(MaxVID);

  // walk backwards towards 'main' and look for the first patchable function.
  for (auto IDI = CxtIDs.rbegin(); IDI != CxtIDs.rend(); IDI++) {
    auto VI = CCT.getInfo(*IDI);
    if (VI.isPatchable())
      return VI.getFuncName();
  }

  return llvm::None;
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