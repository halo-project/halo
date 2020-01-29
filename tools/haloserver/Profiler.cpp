#include "halo/compiler/Profiler.h"

#include "halo/server/ClientSession.h"
#include "Messages.pb.h"

namespace halo {

llvm::Optional<std::pair<std::string, bool>>
  Profiler::getMostSampled(std::list<std::unique_ptr<ClientSession>> &Clients) {

  // FIXME: for now, the most sampled function across all clients, breaking
  // ties arbitrarily (by appearance in list). Not a good metric!
  size_t Max = 0;
  std::string BestName = "";
  bool BestPatchable = false;

  for (auto &CS : Clients) {
    auto &CRI = CS->State.CRI;
    auto &PerfData = CS->State.PerfData;
    for (auto &Pair : CRI.getNameMap()) {
      auto FI = Pair.second;
      auto Name = FI->getName();
      assert(Pair.first == Name && "inconsistency in the name map");

      // FIXME: kind of an awful metric, and we shouldn't just delete the data!
      // plus it's n^2 time to compute this hotness!
      size_t Hotness = 0;
      for (auto const& Sample : PerfData.getSamples()) {
        auto SampleInfo = CRI.lookup(Sample.instr_ptr());
        if (SampleInfo->getName() == Name)
          Hotness++;
      }

      if (Hotness > Max) {
        bool ThisPatchability = FI->isPatchable();
        if ((BestPatchable == false && ThisPatchability == true) || BestPatchable == ThisPatchability) {
          BestName = Name;
          BestPatchable = ThisPatchability;
          Max = Hotness;
        }
      }
    }

    PerfData.getSamples().clear(); // FIXME: get rid of this bad way weigh recency
  }

  if (Max > 0)
    return std::make_pair(BestName, BestPatchable);

  return llvm::None;
}

void Profiler::dump(llvm::raw_ostream &out) {
  fatal_error("implement Profiler::dump!");
}

} // end namespace halo