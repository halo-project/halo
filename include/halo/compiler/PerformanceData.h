#pragma once

#include "halo/compiler/CodeRegionInfo.h"
#include "Messages.pb.h"

#include <list>

namespace halo {

class PerformanceData {
public:

  void add(std::vector<pb::RawSample> const& Samples);
  void add(pb::RawSample const& RS);

  void add(pb::XRayProfileData const&);

  auto& getSamples() { return Samples; }
  auto& getEvents() { return Events; }

  auto const& getSamples() const { return Samples; }
  auto const& getEvents() const { return Events; }

  // clears all data contained
  void clear();

private:
  std::list<pb::RawSample> Samples;
  std::list<pb::XRayProfileData> Events;
};

}
