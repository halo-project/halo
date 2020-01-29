#pragma once

#include "Messages.pb.h"
#include "halo/server/ClientGroup.h"

#include <iostream>
#include <unordered_map>
#include <utility>

namespace halo {

class Profiler {
public:
  // returns the name of the most sampled function and whether it is patchable.
  // patchable functions are prioritized over non-patchable and will always be returned
  // if sampled.
  llvm::Optional<std::pair<std::string, bool>> getMostSampled(std::list<std::unique_ptr<ClientSession>> &Clients) {

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

  void dump(std::ostream &out) {
    fatal_error("implement Profiler::dump!");
  }


private:

};

}
