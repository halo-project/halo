#pragma once

#include "halo/compiler/CodeRegionInfo.h"
#include "Messages.pb.h"

#include <vector>

namespace halo {

struct PerformanceData {
  CodeRegionInfo CRI;

  void init(pb::ClientEnroll &C);

  void add(std::vector<pb::RawSample> const& Samples);
  void add(pb::RawSample const& RS);
};

}
