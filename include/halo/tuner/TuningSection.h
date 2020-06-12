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
  /// Creates a code version corresponding to the original library in the client.
  CodeVersion() : LibName(CodeRegionInfo::OriginalLib) {
    std::fill(ObjFileHash.begin(), ObjFileHash.end(), 0);
  }

  /// Code version for original lib, along with its config.
  CodeVersion(KnobSet OriginalConfig) : LibName(CodeRegionInfo::OriginalLib) {
    std::fill(ObjFileHash.begin(), ObjFileHash.end(), 0);
    Configs.push_back(std::move(OriginalConfig));
  }

  // Create a code version for a finished job.
  CodeVersion(CompilationManager::FinishedJob &&Job);

  std::string const& getLibraryName() const { return LibName; }

  std::unique_ptr<llvm::MemoryBuffer> const& getObjectFile() const { return ObjFile; }

  /// returns true if the given code version was merged with this code version.
  /// The check is performed by comparing the object files for equality.
  /// If they're equal, this code version has its configs extended with the other's.
  bool tryMerge(CodeVersion &CV) {
    if (ObjFileHash != CV.ObjFileHash)
      return false;

    for (auto KS : CV.Configs)
      Configs.push_back(std::move(KS));

    // TODO: should we merge other stuff, like IPC?
    // currently I only see calling this on a fresh CV.
    assert(CV.IPC.observations() == 0 && "see TODO above");

    CV.Configs.clear();
    return true;
  }

  bool isBroken() const { return Broken; }

  bool isOriginalLib() const { return LibName == CodeRegionInfo::OriginalLib; }

  void observeIPC(double value) { IPC.observe(value); }

  size_t recordedIPCs() const { return IPC.observations(); }

  // returns true if this code version is better than the given one, and false otherwise.
  // if the query cannot be answered, then NONE is returned instead.
  llvm::Optional<bool> betterThan(CodeVersion const& Other) const {
    auto Me = IPC.mean();
    auto Them = Other.IPC.mean();

    return Me >= Them;
  }

  private:
  bool Broken{false};
  std::string LibName;
  std::unique_ptr<llvm::MemoryBuffer> ObjFile;
  std::array<uint8_t, 20> ObjFileHash;
  std::vector<KnobSet> Configs;
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
  AggressiveTuningSection(TuningSectionInitializer TSI, std::string RootFunc);
  void take_step(GroupState &) override;
  void dump() const override;

private:
  enum class ActivityState {
    Ready,
    WaitingForCompile,
    TestingNewLib
  };

  std::string stateToString(ActivityState S) const {
    switch(S) {
      case ActivityState::Ready:                return "READY";
      case ActivityState::WaitingForCompile:    return "COMPILING";
      case ActivityState::TestingNewLib:        return "BAKEOFF";
    };
    return "?";
  }

  std::random_device rd;
  std::mt19937_64 gen;


  static constexpr uint64_t EXPLOIT_FACTOR = 10; // the number of steps we need to exploit to repay our debt
  uint64_t ExploitSteps{0};
  size_t SamplesLastTime{0};

  // statistics for myself during development!!
  uint64_t Steps{0};
  uint64_t Experiments{0};
  uint64_t SuccessfulExperiments{0};
  uint64_t DuplicateCompiles{0};

  ActivityState Status{ActivityState::Ready};
  std::string CurrentLib;
  std::string PrevLib;

  std::unordered_map<std::string, CodeVersion> Versions;
};

} // end namespace