#pragma once

#include "Messages.pb.h"
#include "halo/compiler/CodeRegionInfo.h"

namespace halo {

using ClientID = size_t;

class ExecutionTimeProfiler {
public:
  void observe(ClientID ID, CodeRegionInfo const& CRI, std::vector<pb::CallCountData> const&);
  void observeOne(ClientID ID, CodeRegionInfo const& CRI, pb::CallCountData const&);
};

}