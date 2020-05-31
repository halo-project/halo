#pragma once

#include "halo/compiler/Profiler.h"
#include "halo/compiler/CompilationPipeline.h"
#include "halo/tuner/Actions.h"
#include "halo/tuner/KnobSet.h"
#include "halo/tuner/RandomTuner.h"
#include "halo/server/ThreadPool.h"
#include "halo/server/CompilationManager.h"
#include "halo/nlohmann/json_fwd.hpp"

#include "Logging.h"

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
  TuningSection(TuningSectionInitializer TSI, std::string RootFunc)
    : RootFunc(RootFunc), Compiler(TSI.Pool, TSI.Pipeline), Bitcode(TSI.Bitcode), Profile(TSI.Profile) {
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
  AggressiveTuningSection(TuningSectionInitializer TSI, std::string RootFunc)
            : TuningSection(TSI, RootFunc), gen(rd()) {}
  void take_step(GroupState &) override;
  void dump() const override;

private:
  std::random_device rd;
  std::mt19937_64 gen;
  uint64_t Steps{0};
  bool Waiting{false};
};

} // end namespace