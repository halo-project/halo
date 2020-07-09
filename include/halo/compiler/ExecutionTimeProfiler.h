#pragma once

#include "Messages.pb.h"
#include "halo/compiler/CodeRegionInfo.h"

#include <unordered_map>

namespace halo {

using ClientID = size_t;

class ExecutionTimeProfiler {
public:
  void observe(ClientID ID, CodeRegionInfo const& CRI, std::vector<pb::CallCountData> const&);
  void observeOne(ClientID ID, CodeRegionInfo const& CRI, pb::CallCountData const&);

private:

  struct Data {
    uint64_t Timestamp{0};
    uint64_t CallCount{0};
  };

  using State = std::unordered_map<std::string, Data>;

  std::unordered_map<ClientID, State> Last;
  std::unordered_map<std::string, double> AvgTime;
};

}