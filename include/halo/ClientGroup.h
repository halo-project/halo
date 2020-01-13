#pragma once

#include <memory>
#include <list>
#include <utility>
#include <future>
#include <functional>
#include <string>

#include "halo/ClientSession.h"
#include "halo/CompilationPipeline.h"
#include "halo/TaskQueueOverlay.h"
#include "halo/ThreadPool.h"
#include "halo/SequentialAccess.h"
#include "halo/Profiler.h"
#include "halo/KnobSet.h"

#include "llvm/Support/MemoryBuffer.h"

#include "halo/nlohmann/json_fwd.hpp"

using JSON = nlohmann::json;

namespace halo {

struct GroupState {
  using ClientCollection = std::list<std::unique_ptr<ClientSession>>;

  ClientCollection Clients;
  std::list<std::pair<std::string,
                      std::future<CompilationPipeline::compile_expected>
                     >
           > InFlight;
};


// A group is a set of clients that are equal with respect to:
//
//  1. Bitcode
//  2. Target Triple
//  3. Data Layout
//  4. Host CPU  ??? Not sure if we want to be this discriminatory.
//  5. Other compilation flags.
//
class ClientGroup : public SequentialAccess<GroupState> {
public:
  std::atomic<size_t> NumActive;
  std::atomic<bool> ServiceLoopActive;
  std::atomic<bool> ShouldStop;

  // Construct a singleton client group based on its initial member.
  ClientGroup(JSON const& Config, ThreadPool &Pool, ClientSession *CS, std::array<uint8_t, 20> &BitcodeSHA1);

  // returns true if the session became a member of the group.
  bool tryAdd(ClientSession *CS, std::array<uint8_t, 20> &BitcodeSHA1);

  // Drop dead / disconnected clients.
  void cleanup_async();

  // kicks off a continuous service loop for this group.
  void start_services();

  std::future<void> eachClient(std::function<void(ClientSession&)> Callable) {
    return withState([Callable] (GroupState& State) {
      for (auto &Client : State.Clients)
        Callable(*Client);
    });
  }

  std::future<void> withClientState(ClientSession *CS, std::function<void(SessionState&)> Callable) {
    return withState([CS, Callable] (GroupState& State) {
      Callable(CS->State);
    });
  }

private:

  void run_service_loop();
  void end_service_iteration();

  ThreadPool &Pool;
  KnobSet Knobs;
  CompilationPipeline Pipeline;
  std::unique_ptr<std::string> BitcodeStorage;
  std::unique_ptr<llvm::MemoryBuffer> Bitcode;
  std::set<std::string> FuncsWithBitcode;
  Profiler Profile;
  std::array<uint8_t, 20> BitcodeHash;

};

} // end namespace
