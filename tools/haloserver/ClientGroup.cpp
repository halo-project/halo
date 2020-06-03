
#include "halo/server/ClientGroup.h"
#include "halo/tuner/Utility.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Triple.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/raw_ostream.h"

#include "rl.hpp"

#include "Logging.h"

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


  void ClientGroup::run_service_loop() {
    withState([this] (GroupState &State) {

      // Update the profiler with new PerfData, if any.
      Profile.consumePerfData(State.Clients);

      // age the data
      Profile.decay();

      // Do we need to create a tuning section?
      if (TS == nullptr) {

        if (Profile.samplesConsumed() < 100)
          return end_service_iteration(); // not enough samples to create a TS

        auto MaybeTS = TuningSection::Create(TuningSection::Strategy::Aggressive,
                                              {Config, Pool, Pipeline, Profile, *Bitcode});
        if (!MaybeTS)
          return end_service_iteration(); // no suitable tuning section... nothing to do

        TS = std::move(MaybeTS.getValue());
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

  // turn on sampling right away
  pb::SamplePeriod SP;
  SP.set_period(Profile.getSamplePeriod());
  CS->Chan.send_proto(msg::SetSamplingPeriod, SP);
  CS->Chan.send(msg::StartSampling);

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


ClientGroup::ClientGroup(JSON const& Config, ThreadPool &Pool, ClientSession *CS, std::array<uint8_t, 20> &BitcodeSHA1)
    : SequentialAccess(Pool), NumActive(1), ServiceLoopActive(false),
      ShouldStop(false), ServiceIterationRate(250), Pool(Pool), Config(Config), BitcodeHash(BitcodeSHA1) {

      if (!CS->Enrolled)
        llvm::report_fatal_error("was given a non-enrolled client!");

      pb::ClientEnroll &Client = CS->Client;

      // TODO: parse the build flags and perhaps have client
      // send llvm::sys::getHostCPUFeatures() info for -mattr flags.
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
