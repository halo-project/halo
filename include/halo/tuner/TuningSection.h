#pragma once

#include "halo/compiler/CodeRegionInfo.h"
#include "halo/compiler/CompilationPipeline.h"
#include "halo/compiler/Profiler.h"
#include "halo/tuner/Actions.h"
#include "halo/tuner/KnobSet.h"
#include "halo/tuner/RandomTuner.h"
#include "halo/tuner/RandomQuantity.h"
#include "halo/server/ThreadPool.h"
#include "halo/server/CompilationManager.h"
#include "halo/nlohmann/json_fwd.hpp"

#include "Logging.h"

#include <unordered_set>

using JSON = nlohmann::json;

namespace halo {

class GroupState;

/// A compiled instance of a configuration with respect to the bitcode.
/// colloquially a "dylib" or library.
class CodeVersion {
  public:
  CodeVersion() : LibName(CodeRegionInfo::OriginalLib) {}
  CodeVersion(CompilationManager::FinishedJob &&Job) : LibName(Job.UniqueJobName) {
    if (!Job.Result) {
      warning("Compile job failed with an error, library is broken.");
      Broken = true;
    }

    ObjFile = std::move(Job.Result.getValue());
  }

  std::string const& getLibraryName() const { return LibName; }

  std::unique_ptr<llvm::MemoryBuffer> const& getObjectFile() const { return ObjFile; }

  bool isBroken() const { return Broken; }

  bool isOriginalLib() const { return LibName == CodeRegionInfo::OriginalLib; }

  void observeIPC(double value) { IPC.observe(value); }

  size_t recordedIPCs() const { return IPC.observations(); }

  // returns true if this code version is better than the given one, and false otherwise.
  // if the query cannot be answered, then NONE is returned instead.
  llvm::Optional<bool> betterThan(CodeVersion const& Other) const {
    // TODO: use something fancier than this.
    //
    // 1. Also you need to pay attention to the IPC value being tracked in the CCT.
    // I think you should set its discount factor to be higher so that it forgets the
    // previous IPCs quickly.
    //
    // 2. TODO: TODO: Also, you need to provide the _library name_ along with the
    // function group to get the group's performance w.r.t. the library.
    //
    return Other.IPC.mean() < IPC.mean();
  }

  private:
  bool Broken{false};
  std::string LibName;
  std::unique_ptr<llvm::MemoryBuffer> ObjFile;
  RandomQuantity IPC{50};
};


/// There's a lot of junk needed to initialize one of these tuning sections.
struct TuningSectionInitializer {
  JSON const& Config;
  ThreadPool &Pool;
  CompilationPipeline &Pipeline;
  Profiler &Profile;
  llvm::MemoryBuffer &OriginalBitcode;
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
  TuningSection(TuningSectionInitializer TSI, std::string RootFunc);

  // sends the finished compilation job's object file to all clients.
  // It also will not actually send the library if the client already has it.
  void sendLib(GroupState &, CodeVersion const&);

  void redirectTo(GroupState &, CodeVersion const&);

  FunctionGroup FnGroup;
  KnobSet Knobs;
  CompilationManager Compiler;
  std::unique_ptr<llvm::MemoryBuffer> Bitcode;
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
  enum class ActivityState {
    Ready,
    WaitingForCompile,
    TestingNewLib
  };

  std::random_device rd;
  std::mt19937_64 gen;


  static constexpr uint64_t EXPLOIT_FACTOR = 10; // the number of steps we need to exploit to repay our debt
  uint64_t ExploitSteps{0};
  size_t SamplesLastTime{0};

  // statistics for myself
  uint64_t Steps{0};
  uint64_t Experiments{0};



  ActivityState Status{ActivityState::Ready};
  CodeVersion CurrentLib;
  CodeVersion PrevLib;
};

} // end namespace