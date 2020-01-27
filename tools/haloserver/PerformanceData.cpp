#include "halo/compiler/PerformanceData.h"
#include "Logging.h"


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

  void PerformanceData::add(pb::FuncMeasurements const& FM) {
    // TODO: actually do something here
    fatal_error("implement PerformanceDAta::add(pb::FuncMeasurements const&)");
  }

}
