#pragma once

#include "halo/CodeRegionInfo.h"
#include "Messages.pb.h"

#include <iostream>
#include <unordered_map>

namespace halo {

class Profiler {
public:
  CodeRegionInfo CRI;

  void init(pb::ClientEnroll &C) {
    CRI.init(C);
  }

  void analyze(std::vector<pb::RawSample> const& Samples) {
    for (auto &RS : Samples) {
      uint64_t IP = RS.instr_ptr();
      auto Info = CRI.lookup(IP);
      Info->Samples.push_back(RS);
    }
  }

  // returns nullptr if no functions matching the critera exists.
  FunctionInfo* getMostSampled(bool MustHaveBitcode = true) {
    size_t Max = 0;
    FunctionInfo *Hottest = nullptr;

    for (auto &Pair : CRI.NameMap) {
      auto FI = Pair.second;
      auto Num = FI->Samples.size();
      if (Num > Max && (!MustHaveBitcode || FI->HaveBitcode)) {
        Max = Num;
        Hottest = FI;
      }
    }

    return Hottest;
  }

  void dump(std::ostream &out) {
    out << "--- Last Analysis Results ---\n";
    for (auto &Pair : CRI.NameMap) {
      auto FI = Pair.second;
      if (FI->Samples.size() > 0)
        out << FI->Name << " was sampled " << FI->Samples.size() << " times.\n";
    }
    out << "-------\n";
  }


private:

};

}
