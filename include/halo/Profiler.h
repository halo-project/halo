#pragma once

#include "Messages.pb.h"
#include "halo/ClientGroup.h"

#include <iostream>
#include <unordered_map>

namespace halo {

class Profiler {
public:
  // returns nullptr if no functions matching the critera exists.
  llvm::Optional<llvm::StringRef> getMostSampled(std::list<std::unique_ptr<ClientSession>> &Clients) {

    // for now, the most sampled function across all clients, breaking
    // ties arbitrarily (by appearance in list). Not a good metric! FIXME
    size_t Max = 0;
    llvm::StringRef BestName = "";
    for (auto &CS : Clients) {
      auto &CRI = CS->State.Data.CRI;
      for (auto &Pair : CRI.NameMap) {
        auto FI = Pair.second;
        auto Name = FI->Name;

        if (Name == "main" || Name == CodeRegionInfo::UnknownFn)
          continue;

        size_t Num = FI->Samples.size();
        if (Num > Max) {
          BestName = FI->Name;
          Max = Num;
        }
      }
    }

    if (Max > 0)
      return BestName;

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
