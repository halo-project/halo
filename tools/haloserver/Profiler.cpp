#include "halo/compiler/Profiler.h"

#include "halo/server/ClientSession.h"
#include "Messages.pb.h"

namespace halo {

void Profiler::consumePerfData(ClientList & Clients) {
  for (auto &CS : Clients) {
    auto &State = CS->State;
    CCT.observe(State.ID, State.CRI, State.PerfData);
    State.PerfData.clear();
  }
}

void Profiler::decay() {
  CCT.decay();
}

llvm::Optional<Profiler::TuningSection> Profiler::getBestTuningSection() {
  // find the hottest VID
  using VertexID = CallingContextTree::VertexID;
  float Max = 0.0f;
  VertexID MaxVID = CCT.reduce<VertexID>([&](VertexID ID, VertexInfo const& VI, VertexID AccID)  {
    auto Hotness = VI.getHotness();
    if (Hotness > Max) {
      Max = Hotness;
      return ID;
    }
    return AccID;
  }, CCT.getRoot());

  // obtain the context of that VID
  auto Cxt = CCT.contextOf(MaxVID);

  // for now we just walk backwards towards 'main' and look for the first patchable function.
  for (auto I = Cxt.rbegin(); I != Cxt.rend(); I++)
    if (I->isPatchable())
      return std::make_pair(I->getFuncName(), true);

  return llvm::None;
}

void Profiler::dump(llvm::raw_ostream &out) {
  fatal_error("implement Profiler::dump!");
}

} // end namespace halo