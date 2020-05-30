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

/// There's a lot of junk needed to initialize one of these tuning sections.
struct TuningSectionInitializer {
  JSON const& Config;
  ThreadPool &Pool;
  CompilationPipeline &Pipeline;
  Profiler &Profile;
  llvm::MemoryBuffer &Bitcode;
  std::string RootFunc;
};


class TuningSection {
public:

  enum class Strategy {
    Aggressive
  };

  /// @returns a fresh tuning section that is selected based on the current profiling data.
  static llvm::Optional<std::unique_ptr<TuningSection>> Create(Strategy, TuningSectionInitializer);

  /// incrementally tunes the tuning section for the given set of clients, etc.
  virtual void take_step(GroupState &) {
    fatal_error("you should override the base impl of take_step.");
  }

  virtual void dump() const {
    fatal_error("you should override the base impl of dump.");
  };

protected:
  TuningSection(TuningSectionInitializer TSI)
    : RootFunc(TSI.RootFunc), Compiler(TSI.Pool, TSI.Pipeline), Bitcode(TSI.Bitcode), Profile(TSI.Profile) {
    KnobSet::InitializeKnobs(TSI.Config, Knobs);
  }

  // TODO: needs a better interface.
  bool trySendCode(GroupState &);

  std::string RootFunc; // the name of a patchable function serving as the root of this tuning section.
  KnobSet Knobs;
  CompilationManager Compiler;
  llvm::MemoryBuffer& Bitcode;
  Profiler &Profile;
};


/// haven't decided on what this should do yet.
class AggressiveTuningSection : public TuningSection {
public:
  AggressiveTuningSection(TuningSectionInitializer TSI) : TuningSection(TSI) {}
  void take_step(GroupState &) override;
  void dump() const override;

private:
  bool ShouldCompile{true};
  bool CodeSent{false};
};

} // end namespace