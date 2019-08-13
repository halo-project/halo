
#include "llvm/Support/CommandLine.h"

#include "boost/asio.hpp"

#include "halo/ClientRegistrar.h"

#include <cinttypes>
#include <iostream>


namespace cl = llvm::cl;
namespace asio = boost::asio;
namespace ip = boost::asio::ip;

/////////////
// Command-line Options
cl::opt<uint32_t> CL_Port("port",
                       cl::desc("TCP port to listen on."),
                       cl::init(29000));

//////////
// These options are mainly to aid in building a test suite.

cl::opt<bool> CL_NoPersist("no-persist",
                      cl::desc("Gracefully quit once all clients have disconnected."),
                      cl::init(false));

// NOTE: the timeout is really "at least N seconds". This is meant to prevent the
// test suite from running forever, so set it much higher than you expect to need.
cl::opt<uint32_t> CL_TimeoutSec("timeout",
                      cl::desc("Quit with non-zero exit code if N seconds have elapsed. Zero means disabled."),
                      cl::init(0));

namespace halo {

// TODO: this needs to move into its own module.

void service_session(ClientSession &CS) {
  if (!CS.Measuring) {
    // 1. determine which function is the hottest for this session.
    FunctionInfo *FI = CS.Profile.getMostSampled();
    if (FI) {
      std::cout << "Hottest function = " << FI->Name << "\n";

      // 2. send a request to client to begin timing the execution of the function.
      CS.Measuring = true;
      pb::ReqMeasureFunction MF;
      MF.set_func_addr(FI->AbsAddr);
      CS.Chan.send_proto(msg::ReqMeasureFunction, MF);

      // 3. compile a new version in parallel for now.

    }
  }
}

void service_group(ClientGroup &G) {
  // since this function is run in the IOService thread, it should avoid blocking
  // for too long.
  G.apply(service_session);
}

} // end namespace halo


int main(int argc, char* argv[]) {
  cl::ParseCommandLineOptions(argc, argv, "Halo Server\n");

  asio::io_service IOService;

  halo::ClientRegistrar CR(IOService, CL_Port, CL_NoPersist);

  std::thread io_thread([&](){ IOService.run(); });

  std::cout << "Started Halo Server.\nListening on port "
            << CL_Port << std::endl;

  const uint32_t SleepMS = 50; // Lower this to be more aggressive.
  const bool TimeLimited = CL_TimeoutSec > 0;
  int64_t RemainingTime = CL_TimeoutSec * 1000;
  bool ForceShutdown = false;

  // The main event loop that drives actions performed by the server (i.e.,
  // not just in response to clients). This is the heartbeat of the system.
  do {
    std::this_thread::sleep_for(std::chrono::milliseconds(SleepMS));

    if (TimeLimited && !ForceShutdown) {
      RemainingTime -= SleepMS;
      ForceShutdown = RemainingTime <= 0;
      if (ForceShutdown)
        std::cerr << "Server's running time limit reached. Shutting down.\n";
    }

    // Modifications to the CR's state should occur in the IOService thread.
    IOService.dispatch([&](){
      CR.cleanup();

      if (CR.consider_shutdown(ForceShutdown))
        return;

      CR.apply(halo::service_group);
    });
  } while (!IOService.stopped());

  io_thread.join();

  if (ForceShutdown)
    return 1;

  return 0;
}
