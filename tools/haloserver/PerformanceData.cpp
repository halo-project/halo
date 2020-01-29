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

  void PerformanceData::add(pb::XRayProfileData const& FM) {
    Events.push_back(FM);
  }

  void PerformanceData::clear() {
    Samples.clear();
    Events.clear();
  }

}
