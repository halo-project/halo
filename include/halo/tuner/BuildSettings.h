#pragma once

#include "llvm/Passes/PassBuilder.h"

namespace halo {

  // a representation of various build settings found in the original
  // executable. This is used to initialize the knob set for the "original" lib
  struct BuildSettings {
    llvm::PassBuilder::OptimizationLevel OptLvl = llvm::PassBuilder::OptimizationLevel::O0;
  };

} // end namespace