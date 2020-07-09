#pragma once

#include "halo/compiler/CodeRegionInfo.h"
#include "Messages.pb.h"

#include <vector>

namespace halo {

class PerformanceData {
public:

  void add(std::vector<pb::RawSample> const& Samples);
  void add(pb::RawSample const& RS);

  void add(pb::CallCountData const&);

  auto& getSamples() { return Samples; }
  auto& getCallCounts() { return CallCounts; }

  auto const& getSamples() const { return Samples; }
  auto const& getCallCounts() const { return CallCounts; }

  // clears all data contained
  void clear();

private:
  std::vector<pb::RawSample> Samples;
  std::vector<pb::CallCountData> CallCounts;
};

}
