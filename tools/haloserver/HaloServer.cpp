
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"

#include "halo/nlohmann/util.hpp"

#include "boost/asio.hpp"

#include "halo/server/ClientRegistrar.h"

#include <cinttypes>
#include <fstream>
#include <iomanip>


namespace cl = llvm::cl;
namespace asio = boost::asio;
namespace ip = boost::asio::ip;

using JSON = nlohmann::json;

/////////////
// Command-line Options
static cl::opt<uint32_t> CL_Port("halo-port",
                       cl::desc("TCP port to listen on. (default = 29000)"),
                       cl::init(29000));

//////////
// These options are mainly to aid in building a test suite.

static cl::opt<bool> CL_NoPersist("halo-no-persist",
                      cl::desc("Gracefully quit once all clients have disconnected. (default = false)"),
                      cl::init(false));

// NOTE: the timeout is really "at least N seconds". This is meant to prevent the
// test suite from running forever, so set it much higher than you expect to need.
static cl::opt<uint32_t> CL_TimeoutSec("halo-timeout",
                      cl::desc("Quit with non-zero exit code if N seconds have elapsed. (default = 0 which means disabled)"),
                      cl::init(0));

static cl::opt<std::string> CL_ConfigPath("halo-config",
                      cl::desc("Specify path to the JSON-formatted configuration file. By default searches for server-config.json next to executable."),
                      cl::init(""));

static cl::opt<std::string> CL_Strategy("halo-strategy",
                      cl::desc("The TuningSection strategy to use. (options are: aggressive, jitonce)"),
                      cl::init("aggressive"));

static cl::opt<unsigned> CL_NumThreads("halo-threads",
                      cl::desc("Maximum number of threads to use. (default = 0 means use all available)"),
                      cl::init(0));

namespace halo {

void service_group(ClientGroup &G) {
    G.start_services();
}

JSON ReadConfigFile(const char* Path) {
  // Read the configuration file.
  std::ifstream file(Path);

  if (!file.is_open()) {
    clogs() << "Unable to open server config file: " << Path << std::endl;
    halo::fatal_error("exiting due to previous error");
  }

  JSON ServerConfig = JSON::parse(file, nullptr, false);
  if (ServerConfig.is_discarded()) {
    // would be nice if there was a way to get information about where it is
    // without using exceptions, but that doesn't seem possible right now.
    halo::fatal_error("syntax error in JSON file");
  }

  logs() << "Using the server config: " << Path << "\n";
            // << std::setw(2) << ServerConfig << std::endl;

  return ServerConfig;
}

} // end namespace halo


int main(int argc, char* argv[]) {
  // Verify that the version of the library that we linked against is
  // compatible with the version of the headers we compiled against.
  GOOGLE_PROTOBUF_VERIFY_VERSION;


  cl::ParseCommandLineOptions(argc, argv, "Halo Server\n");

  llvm::SmallString<256> Path;

  // if no path is given, look in the directory where the
  // executable lives for a server-config.json file.
  if (CL_ConfigPath == "") {
    // Set path to be the directory where this executable lives.
    Path = llvm::sys::fs::getMainExecutable(argv[0], (void*)&main);
    Path = Path.substr(0, Path.rfind('/')); // drop the '/haloserver' from end

    // assume the filename.
    Path += "/server-config.json";
  } else {
    // otherwise, just expand tildes before trying to open the given path.
    llvm::sys::fs::expand_tilde(CL_ConfigPath, Path);
  }

  JSON ServerConfig = halo::ReadConfigFile(Path.c_str());

  // Initialize parts of LLVM related to JIT compilation.
  // See llvm/Support/TargetSelect.h for other items.
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();
  // llvm::InitializeAllDisassemblers(); // might be handy for debugging


  asio::io_service IOService;

  halo::config::setServerSetting("strategy", CL_Strategy, ServerConfig);

  halo::ClientRegistrar CR(IOService, CL_Port, CL_NoPersist, CL_NumThreads, ServerConfig);

  std::thread io_thread([&](){ IOService.run(); });

  halo::logs() << "Started Halo Server.\nListening on port "
            << CL_Port << "\n";

  // This rate controls how rapidly the entire system takes actions
  const size_t BeatsPerSecond = halo::config::getServerSetting<size_t>("heartbeats-per-second", ServerConfig);

  const uint32_t SleepMS = BeatsPerSecond == 0 ? 0 : 1000 / BeatsPerSecond;
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
