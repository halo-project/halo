
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Error.h"

#include "halo/nlohmann/json.hpp"

#include "boost/asio.hpp"

#include "halo/ClientRegistrar.h"

#include <cinttypes>
#include <fstream>
#include <iomanip>


namespace cl = llvm::cl;
namespace asio = boost::asio;
namespace ip = boost::asio::ip;

using JSON = nlohmann::json;

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

cl::opt<std::string> CL_ConfigPath("config",
                      cl::desc("Path to the JSON-formatted configuration file."),
                      cl::init("./server-config.json"));

namespace halo {

void service_group(ClientGroup &G) {
    G.start_services();
}

JSON ReadConfigFile(std::string &Path) {
  // Read the configuration file.
  std::ifstream file(Path);

  if (!file.is_open()) {
    std::cerr << "Unable to open server config file: " << Path << std::endl;
    llvm::report_fatal_error("exiting due to previous error");
  }

  JSON ServerConfig = JSON::parse(file, nullptr, false);
  if (ServerConfig.is_discarded()) {
    // would be nice if there was a way to get information about where it is
    // without using exceptions, but that doesn't seem possible right now.
    llvm::report_fatal_error("syntax error in JSON file");
  }

  std::cout << "Read the following server config:\n"
            << std::setw(2) << ServerConfig << std::endl;

  return ServerConfig;
}

} // end namespace halo


int main(int argc, char* argv[]) {
  // Verify that the version of the library that we linked against is
  // compatible with the version of the headers we compiled against.
  GOOGLE_PROTOBUF_VERIFY_VERSION;


  cl::ParseCommandLineOptions(argc, argv, "Halo Server\n");

  JSON ServerConfig = halo::ReadConfigFile(CL_ConfigPath);

  // TODO: remove this, it's just for testing
  halo::KnobSet S;
  halo::KnobSet::InitializeKnobs(ServerConfig, S);

  // Initialize parts of LLVM related to JIT compilation.
  // See llvm/Support/TargetSelect.h for other items.
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();
  // llvm::InitializeAllDisassemblers(); // might be handy for debugging


  asio::io_service IOService;

  halo::ClientRegistrar CR(IOService, CL_Port, CL_NoPersist, ServerConfig);

  std::thread io_thread([&](){ IOService.run(); });

  halo::log() << "Started Halo Server.\nListening on port "
            << CL_Port << "\n";

  const uint32_t SleepMS = 500; // Lower this to be more aggressive.
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
        halo::info("Server's running time limit reached. Shutting down.\n");
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
