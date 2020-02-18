//===----------------------------------------------------------------------===//
/// \file
/// This file defines a Halo pass to analyze the module to aid in profiling.
//===----------------------------------------------------------------------===//

#pragma once

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

namespace halo {
  class Profiler;
}

namespace llvm {

  /// A module pass for analyzing the module to populate the Halo Profiler
  /// with static program information.
  class ProgramInfoPass : public PassInfoMixin<ProgramInfoPass> {
  public:
    ProgramInfoPass(halo::Profiler &Prof) : Profiler(Prof) {}

    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);

  private:
      halo::Profiler &Profiler;
  };
} // end namespace llvm