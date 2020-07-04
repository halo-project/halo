#pragma once

#include "halo/compiler/Profiler.h"
#include "halo/tuner/KnobSet.h"
#include "halo/tuner/CodeVersion.h"
#include "halo/server/CompilationManager.h"
#include "halo/nlohmann/json_fwd.hpp"

#include "Logging.h"

#include <unordered_set>

using JSON = nlohmann::json;

namespace halo {

class GroupState;
class Bakeoff;
class CompilationPipeline;
class BuildSettings;
class ThreadPool;


/// There's a lot of junk needed to initialize one of these tuning sections.
struct TuningSectionInitializer {
  JSON const& Config;
  ThreadPool &Pool;
  CompilationPipeline &Pipeline;
  Profiler &Profile;
  llvm::MemoryBuffer &OriginalBitcode;
  BuildSettings &OriginalSettings;
};

namespace Strategy {
  enum Kind {
    Aggressive,
    JitOnce
  };
}


class TuningSection {
public:

  /// @returns a fresh tuning section that is selected based on the current profiling data.
  static llvm::Optional<std::unique_ptr<TuningSection>> Create(TuningSectionInitializer);

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



} // end namespace