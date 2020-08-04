#pragma once

#include <set>

namespace halo {

  enum RootAction {
    RunExperiment,
    Wait
  };

  const static std::set<RootAction> RootActions = {
    RootAction::RunExperiment,
    RootAction::Wait
  };


} // end namespace halo