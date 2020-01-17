#pragma once

#include <set>

namespace halo {

  enum RootAction {
    Optimize,
    DoNothing
  };

  const static std::set<RootAction> RootActions = {
    RootAction::Optimize,
    RootAction::DoNothing
  };


} // end namespace halo