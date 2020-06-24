
#include "halo/server/ClientGroup.h"
#include "halo/tuner/Utility.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Triple.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/raw_ostream.h"

#include "halo/nlohmann/util.hpp"

#include "rl.hpp"

#include "Logging.h"

#include <regex>

namespace halo {

  // kicks off a continuous service loop for this group.
  void ClientGroup::start_services() {
    if (ServiceLoopActive)
      return;

    ServiceLoopActive = true;
    run_service_loop();
  }

  void ClientGroup::end_service_iteration() {
    // This method should be called before the service loop function ends
    // to queue up another iteration, otherwise we will be stalled forever.

    if (ShouldStop) {
      ServiceLoopActive = false;
      return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(ServiceIterationRate));
    run_service_loop();
  }

  void ClientGroup::broadcastSamplingPeriod(GroupState &State, uint64_t Period) {
    for (auto &Client : State.Clients)
      Client->set_sampling_period(Client->State, Period);
  }

  bool ClientGroup::identifyTuningSection(GroupState &State) {
    const int N = 4;
    // update with wrap-around
    // we spend 1/N of our the time sampling to identify a TS,
    // and (N-1)/N of our time taking a break.
    IdentifySteps--;
    if (IdentifySteps <= -(N * IDENTIFY_STEP_FACTOR))
      IdentifySteps = IDENTIFY_STEP_FACTOR;

    if (IdentifySteps < 0) {
      // take a break from sampling
      info("identifyTuningSection -- taking a break.");
      broadcastSamplingPeriod(State, 0);
      return false;
    }

    broadcastSamplingPeriod(State, Profile.getSamplePeriod());
    Profile.consumePerfData(State);

    size_t TotalSamples = Profile.samplesConsumed();

    if (TotalSamples < 100)
      return false; // not enough samples to create a TS

    auto MaybeTS = TuningSection::Create(TuningSection::Strategy::Aggressive,
                        {Config, Pool, Pipeline, Profile, *Bitcode, OriginalSettings});
    if (!MaybeTS)
      return false; // no suitable tuning section... nothing to do


    TS = std::move(MaybeTS.getValue());

    return true; // we finally got a tuning section!
  }


  void ClientGroup::run_service_loop() {
    withState([this] (GroupState &State) {

      // Do we need to create a tuning section?
      if (TS == nullptr) {


        if (!identifyTuningSection(State))
          return end_service_iteration();

        // turn sampling back off, since that's the default expected state for TuningSection
        // upon initialization.
        broadcastSamplingPeriod(State, 0);
      }

      TS->take_step(State);

      TS->dump();

      return end_service_iteration();
    }); // end of lambda
  }



void ClientGroup::cleanup_async() {
  withState([&] (GroupState &State) {
    auto &Clients = State.Clients;
    auto it = Clients.begin();
    size_t removed = 0;
    while(it != Clients.end()) {
      if ((*it)->Status == Dead)
        { it = Clients.erase(it); removed++; }
      else
        it++;
    }
    if (removed)
      NumActive -= removed;
  });
}


void ClientGroup::addSession(ClientSession *CS, GroupState &State) {
  CS->start(this);

  State.Clients.push_back(std::unique_ptr<ClientSession>(CS));
}

bool ClientGroup::tryAdd(ClientSession *CS, std::array<uint8_t, 20> &TheirHash) {
  assert(CS->Enrolled);

  if (BitcodeHash != TheirHash)
    return false;

  pb::ClientEnroll &CE = CS->Client;
  if (!Pipeline.getTriple().isCompatibleWith(llvm::Triple(CE.process_triple())))
    return false;

  if (Pipeline.getCPUName() != CE.host_cpu())
    return false;

  NumActive++; // do this in the caller's thread eagarly.
  withState([this,CS] (GroupState &State) {
    addSession(CS, State);
  });

  return true;
}


void analyzeBuildFlags(BuildSettings &Settings, pb::ModuleInfo const& MI) {
  const std::regex OptLevelRegex("-(O[0123])");
  std::smatch pieces_match;

  // based on example from  https://en.cppreference.com/w/cpp/regex/regex_match
  for (auto const& Flag : MI.build_flags()) {
    if (std::regex_match(Flag, pieces_match, OptLevelRegex)) {
      // since we go in order, we'll always end up with the last -On flag.
      std::ssub_match sub_match = pieces_match[1];
      std::string piece = sub_match.str();
      clogs() << "Opt level found in build flags: " << piece << "\n";
      Settings.OptLvl = OptLvlKnob::parseLevel(piece);
    }
  }
}


ClientGroup::ClientGroup(JSON const& Config, ThreadPool &Pool, ClientSession *CS, std::array<uint8_t, 20> &BitcodeSHA1)
    : SequentialAccess(Pool), NumActive(1), ServiceLoopActive(false),
      ShouldStop(false), Pool(Pool), Config(Config), Profile(Config), BitcodeHash(BitcodeSHA1) {

      // the amount of time to sleep before enqueueing another ASIO service iteration.
      size_t ItersPerSec = config::getServerSetting<size_t>("group-service-per-second", Config);
      ServiceIterationRate = ItersPerSec == 0 ? 0 : 1000 / ItersPerSec;

      if (!CS->Enrolled)
        llvm::report_fatal_error("was given a non-enrolled client!");

      pb::ClientEnroll &Client = CS->Client;

      // TODO: have client send llvm::sys::getHostCPUFeatures() info so we can
      // add -mattr flags during compilation.

      analyzeBuildFlags(OriginalSettings, Client.module());

      Pipeline = CompilationPipeline(
                    llvm::Triple(Client.process_triple()),
                    Client.host_cpu());


      // take ownership of the bitcode, and maintain a MemoryBuffer view of it.
      BitcodeStorage = std::move(
          std::unique_ptr<std::string>(Client.mutable_module()->release_bitcode()));
      Bitcode = std::move(
          llvm::MemoryBuffer::getMemBuffer(llvm::StringRef(*BitcodeStorage))
                         );

      Pipeline.analyzeForProfiling(Profile, *Bitcode);

      withState([this,CS] (GroupState &State) {
        addSession(CS, State);
      });
    }

} // namespace halo
