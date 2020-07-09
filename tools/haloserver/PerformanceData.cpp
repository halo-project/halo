#include "halo/compiler/PerformanceData.h"
#include "Logging.h"


namespace halo {

  void PerformanceData::add(std::vector<pb::RawSample> const& Samples) {
    for (auto const& RS : Samples)
      add(RS);
  }

  void PerformanceData::add(pb::RawSample const& RS) {
    Samples.push_back(RS);
  }

  void PerformanceData::add(pb::CallCountData const& CCD) {
    CallCounts.push_back(CCD);
  }

  void PerformanceData::clear() {
    Samples.clear();
    CallCounts.clear();
  }

}
