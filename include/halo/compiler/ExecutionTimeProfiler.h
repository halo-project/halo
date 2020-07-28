#pragma once

#include "Messages.pb.h"
#include "halo/compiler/CodeRegionInfo.h"
#include "llvm/ADT/Optional.h"
#include "halo/nlohmann/json_fwd.hpp"

#include <unordered_map>

using JSON = nlohmann::json;

namespace halo {

using ClientID = size_t;

struct CallFreq {
  double Value{0};  // calls per time unit
  size_t SamplesSeen{0};
  unsigned MilliPerCall{0}; // the time unit: milliseconds per call
};

class ExecutionTimeProfiler {
public:

  ExecutionTimeProfiler(JSON const&);

  void observe(ClientID ID, CodeRegionInfo const& CRI, std::vector<pb::CallCountData> const&);
  void observeOne(ClientID ID, CodeRegionInfo const& CRI, pb::CallCountData const&);

  // returns the average number of calls per time unit
  CallFreq get(std::string const& FuncName);

private:

  struct Data {
    uint64_t Timestamp{0};
    uint64_t CallCount{0};
  };

  using State = std::unordered_map<std::string, Data>;

  const double DISCOUNT;
  std::unordered_map<ClientID, State> Last;
  std::unordered_map<std::string, CallFreq> Data;
};

}