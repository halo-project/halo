#pragma once

#include "halo/compiler/CodeRegionInfo.h"
#include "halo/compiler/CompilationPipeline.h"
#include "halo/compiler/Profiler.h"
#include "halo/tuner/Actions.h"
#include "halo/tuner/Bakeoff.h"
#include "halo/tuner/KnobSet.h"
#include "halo/tuner/PseudoBayesTuner.h"
#include "halo/tuner/RandomQuantity.h"
#include "halo/tuner/CodeVersion.h"
#include "halo/tuner/BuildSettings.h"
#include "halo/server/ThreadPool.h"
#include "halo/server/CompilationManager.h"
#include "halo/nlohmann/json_fwd.hpp"

#include "Logging.h"

#include <unordered_set>

using JSON = nlohmann::json;

namespace halo {

class GroupState;


/// There's a lot of junk needed to initialize one of these tuning sections.
struct TuningSectionInitializer {
  JSON const& Config;
  ThreadPool &Pool;
  CompilationPipeline &Pipeline;
  Profiler &Profile;
  llvm::MemoryBuffer &OriginalBitcode;
  BuildSettings &OriginalSettings;
};



class TuningSection {
public:

  enum class Strategy {
    Aggressive,
    CompileOnce
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
  TuningSection(TuningSectionInitializer TSI, std::string RootFunc);

  friend Bakeoff;

  // sends the finished compilation job's object file to all clients.
  // It also will not actually send the library if the client already has it.
  void sendLib(GroupState &, CodeVersion const&);

  // performs redirection on each client, if the client is not already using that version.
  void redirectTo(GroupState &, CodeVersion const&);

  FunctionGroup FnGroup;
  KnobSet BaseKnobs; // the knobs corresponding to the JSON file & the loops in the code. you generally don't want to modify this!
  KnobSet OriginalLibKnobs; // knobs corresponding to the original executable. a subset of the BaseKnobs.
  CompilationManager Compiler;
  std::unique_ptr<llvm::MemoryBuffer> Bitcode;
  Profiler &Profile;
  std::unordered_map<std::string, CodeVersion> Versions;
};




/// haven't decided on what this should do yet.
class AggressiveTuningSection : public TuningSection {
public:
  AggressiveTuningSection(TuningSectionInitializer TSI, std::string RootFunc);
  void take_step(GroupState &) override;
  void dump() const override;

private:
  enum class ActivityState {
    Ready,
    Paused,   // basically, exploiting
    WaitingForCompile,
    TestingNewLib
  };

  std::string stateToString(ActivityState S) const {
    switch(S) {
      case ActivityState::Ready:                return "READY";
      case ActivityState::Paused:               return "PAUSED";
      case ActivityState::WaitingForCompile:    return "COMPILING";
      case ActivityState::TestingNewLib:        return "BAKEOFF";
    };
    return "?";
  }

  void transitionTo(ActivityState S);

  // adjusts the Exploit factor based on the result of the bakeoff
  void adjustAfterBakeoff(Bakeoff::Result);

  PseudoBayesTuner PBT;

  uint64_t ExploitSteps{1}; // the count-down for steps remaining to perform that "exploit"
  size_t SamplesLastTime{0};
  unsigned DuplicateCompilesInARow{0};

  // statistics for myself during development!!
  uint64_t Steps{0};
  uint64_t Bakeoffs{0};
  uint64_t SuccessfulBakeoffs{0};
  uint64_t BakeoffTimeouts{0};
  uint64_t DuplicateCompiles{0};

  BakeoffParameters BP;
  llvm::Optional<Bakeoff> Bakery;
  ActivityState Status{ActivityState::Ready};
  std::string BestLib;

  // for choosing a new code version
  const unsigned MAX_DUPES_IN_ROW; // max duplicate compiles in a row before we give up.

  // for managing explore-exploit tradeoff
  const float EXPLOIT_LEARNING_RATE;
  const float MAX_TGT_FACTOR;
  const float MIN_TGT_FACTOR;
  float ExploitFactor; // the number of steps to be taken in the next "exploit" phase
};

} // end namespace