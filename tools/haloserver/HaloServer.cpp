
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/TargetSelect.h"

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

void service_group(ClientGroup &G) {
  // since this function is run in the IOService thread, it should avoid blocking
  // for too long.
  G.service_async();
}

} // end namespace halo


int main(int argc, char* argv[]) {
  cl::ParseCommandLineOptions(argc, argv, "Halo Server\n");

  // Initialize parts of LLVM related to JIT compilation.
  // See llvm/Support/TargetSelect.h for other items.
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();
  // llvm::InitializeAllDisassemblers(); // might be handy for debugging


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
