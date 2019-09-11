#include "halo/PerformanceData.h"


namespace halo {

  void PerformanceData::init(pb::ClientEnroll &C) {
    CRI.init(C);
  }

  void PerformanceData::add(std::vector<pb::RawSample> const& Samples) {
    for (auto const& RS : Samples) {
      add(RS);
    }
  }

  void PerformanceData::add(pb::RawSample const& RS) {
    uint64_t IP = RS.instr_ptr();
    auto Info = CRI.lookup(IP);
    Info->Samples.push_back(RS);
  }

}
