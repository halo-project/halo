#pragma once

#include "halo/compiler/Profiler.h"
#include "halo/compiler/CompilationPipeline.h"
#include "halo/tuner/Actions.h"
#include "halo/tuner/KnobSet.h"
#include "halo/server/ThreadPool.h"
#include "halo/server/CompilationManager.h"

#include "Logging.h"


#include "halo/nlohmann/json_fwd.hpp"

using JSON = nlohmann::json;

namespace halo {

class GroupState;

class TuningSection {
public:

  /// @returns a fresh tuning section that is selected based on the current profiling data.
  static llvm::Optional<std::unique_ptr<TuningSection>> Create(
      JSON const&, ThreadPool &, CompilationPipeline &, Profiler &, llvm::MemoryBuffer &Bitcode);

  /// incrementally tunes the tuning section for the given set of clients, etc.
  void take_step(GroupState &);

  void dump() const;

private:
  TuningSection(JSON const& Config, ThreadPool &Pool, CompilationPipeline &Pipeline,
                Profiler &profile, llvm::MemoryBuffer &bitcode, std::string rootFunc)
    : RootFunc(rootFunc), Compiler(Pool, Pipeline), Bitcode(bitcode), Profile(profile) {
    KnobSet::InitializeKnobs(Config, Knobs);
  }

  // NOTE: tmeporary
  bool trySendCode(GroupState &);

  std::string RootFunc; // the name of a patchable function serving as the root of this tuning section.
  KnobSet Knobs;
  CompilationManager Compiler;
  llvm::MemoryBuffer& Bitcode;
  Profiler &Profile;

  // NOTE: temporary
  bool ShouldCompile{true};
  bool CodeSent{false};

};

} // end namespace