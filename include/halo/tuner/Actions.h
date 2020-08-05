#pragma once

#include <set>

namespace halo {

  enum RootAction {
    RA_RunExperiment,
    RA_RetryBest,
    RA_Wait
  };

  const static std::set<RootAction> RootActions = {
    RootAction::RA_RunExperiment,
    RootAction::RA_RetryBest,
    RootAction::RA_Wait
  };


} // end namespace halo