#pragma once

#include "halo/CodeRegionInfo.h"
#include "Messages.pb.h"

#include <iostream>
#include <unordered_map>

namespace halo {

class Profiler {
public:
  CodeRegionInfo CRI;
  pb::ClientEnroll &Client;

  Profiler(pb::ClientEnroll &C) : Client(C) {}

  void init() {
    CRI.init(Client);
  }

  void analyze(std::vector<pb::RawSample> const& Samples) {
    SampleFrequency.clear();

    for (auto &RS : Samples) {
      uint64_t IP = RS.instr_ptr();

      auto Info = CRI.lookup(IP);
      if (Info) {
        auto Name = Info.getValue()->Name;
        SampleFrequency[Name] = SampleFrequency[Name] + 1;
      } else {
        SampleFrequency[UnknownFn] = SampleFrequency[UnknownFn] + 1;
      }
    }
  }

  void dump(std::ostream &out) {
    out << "--- Last Analysis Results ---\n";
    for (auto &Pair : SampleFrequency) {
      out << Pair.first << " was sampled " << Pair.second << " times.\n";
    }
    out << "-------\n";
  }


private:
  const std::string UnknownFn = "<unknown func>";
  std::unordered_map<std::string, unsigned int> SampleFrequency;
};

}
