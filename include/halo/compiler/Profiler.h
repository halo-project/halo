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
  llvm::Optional<std::pair<llvm::StringRef, bool>> getMostSampled(std::list<std::unique_ptr<ClientSession>> &Clients) {

    // FIXME: for now, the most sampled function across all clients, breaking
    // ties arbitrarily (by appearance in list). Not a good metric!
    size_t Max = 0;
    llvm::StringRef BestName = "";
    bool BestPatchable = false;

    for (auto &CS : Clients) {
      auto &CRI = CS->State.Data.CRI;
      for (auto &Pair : CRI.NameMap) {
        auto FI = Pair.second;
        auto Name = FI->Name;

        size_t Num = FI->Samples.size();
        if (Num > Max) {
          bool ThisPatchability = FI->Patchable;
          if ((BestPatchable == false && ThisPatchability == true) || BestPatchable == ThisPatchability) {
            BestName = FI->Name;
            BestPatchable = ThisPatchability;
            Max = Num;
          }
        }

        // FIXME: this is an exceptionally awful way to determine "most sampled
        // since last time we checked" because the samples have timestamps!
        FI->Samples.clear();

      }
    }

    if (Max > 0)
      return std::make_pair(BestName, BestPatchable);

    return llvm::None;

    // size_t Max = 0;
    // FunctionInfo *Hottest = nullptr;
    //
    // for (auto &Pair : CRI.NameMap) {
    //   auto FI = Pair.second;
    //
    //   if (FI->Name == Excluding)
    //     continue;
    //
    //   auto Num = FI->Samples.size();
    //   if (Num > Max && (!MustHaveBitcode || FI->HaveBitcode)) {
    //     Max = Num;
    //     Hottest = FI;
    //   }
    // }
    //
    // return Hottest;
  }

  void dump(std::ostream &out) {
    // out << "--- Last Analysis Results ---\n";
    // for (auto &Pair : CRI.NameMap) {
    //   auto FI = Pair.second;
    //   if (FI->Samples.size() > 0)
    //     out << FI->Name << " was sampled " << FI->Samples.size() << " times.\n";
    // }
    // out << "-------\n";
  }


private:

};

}
