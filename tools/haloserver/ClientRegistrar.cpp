
#include "halo/server/ClientRegistrar.h"
#include "halo/server/ClientGroup.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/SHA1.h"
#include "llvm/Support/CommandLine.h"

#include "Logging.h"
#include "Messages.pb.h"

#include <cinttypes>
#include <memory>
#include <functional>

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace msg = halo::msg;
namespace cl = llvm::cl;

using JSON = nlohmann::json;

static cl::opt<uint32_t> CL_Port("halo-port",
                       cl::desc("TCP port to listen on. (default = 29000)"),
                       cl::init(29000));

static cl::opt<unsigned> CL_NumThreads("halo-threads",
                      cl::desc("Maximum number of compilation threads to use. (default = 0 means use all hardware resources)"),
                      cl::init(0));

//////////
// this option is  mainly to aid in building a test suite.
static cl::opt<bool> CL_NoPersist("halo-no-persist",
                      cl::desc("Gracefully quit once all clients have disconnected. (default = false)"),
                      cl::init(false));

namespace halo {


ClientRegistrar::ClientRegistrar(asio::io_service &service, JSON config)
    : NoPersist(CL_NoPersist),
      Port(CL_Port),
      ServerConfig(config),
      IOService(service),
      Endpoint(ip::tcp::v4(), Port),
      Acceptor(IOService, Endpoint),
      Pool(CL_NumThreads) {
        accept_loop();
        logs() << "Started Halo Server.\nListening on port " << Port << "\n";
      }

// only safe to call within the IOService. Use io_service::dispatch.
// Because the IOService thread can modify the Groups list.
void ClientRegistrar::cleanup() {
  for (auto &Group : Groups)
    Group.cleanup_async();
}

bool ClientRegistrar::consider_shutdown(bool ForcedShutdown) {
  if (ForcedShutdown)
    goto DO_SHUTDOWN;

  if (NoPersist && TotalSessions > 0) {
    // if we've had at least one client connect, but have no active
    // sessions right now, then we shutdown.

    if (UnregisteredSessions == 0) {

      // look for at least one active client.
      bool OneActive = false;
      for (auto &Group : Groups) {
        size_t Active = Group.NumActive;
        // clogs() << "Group has " << Active << " active.\n";
        if (Active > 0) {
          OneActive = true;
          break;
        }
      }

      if (!OneActive)
        goto DO_SHUTDOWN;

    }
  }

  return false; // no shutdown

DO_SHUTDOWN:
  // notify groups they should not queue up another service iteration
  for (auto &Group : Groups)
    Group.ShouldStop = true;

  // wait until groups have signalled that they're not queueing up more work
  for (auto &Group : Groups)
    while (Group.ServiceLoopActive)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));

  // Kill all connections. This will send an RST packet to clients.
  IOService.stop();
  return true;
}

void ClientRegistrar::accept_loop() {
  // Not a unique_ptr because good luck moving one of those into the lambda!
  auto CS = new ClientSession(IOService, Pool);

  auto &Socket = CS->Socket;
  Acceptor.async_accept(Socket,
    [this,CS](boost::system::error_code Err) {
      if(!Err) {
        info("Received a new connection request.");

        TotalSessions++; UnregisteredSessions++;
        CS->Status = Active;
        CS->ID = TotalSessions;
        asio::socket_base::keep_alive option(true);
        CS->Socket.set_option(option);

        register_loop(CS);
      }
      accept_loop();
    });
}

void ClientRegistrar::register_loop(ClientSession *CS) {
  // NOTE: It's only ok to access Groups here because we know the IOService
  // of the client registrar = IOService of all clients.
  auto &Chan = CS->Chan;
  Chan.async_recv([this,CS](msg::Kind Kind, std::vector<char>& Body) {
    if (Kind == msg::Shutdown) {
      // It never made it into a group, so we clean it up.
      info("Client shutdown before finishing registration.\n");
      CS->shutdown();
      delete CS;
      UnregisteredSessions--;

    } else if (Kind == msg::ClientEnroll) {
      llvm::StringRef Blob(Body.data(), Body.size());

      CS->Client.ParseFromString(Blob.str());
      CS->Enrolled = true;

      llvm::StringRef Bitcode(CS->Client.module().bitcode());
      std::array<uint8_t, 20> Hash = llvm::SHA1::hash(llvm::arrayRefFromStringRef(Bitcode));

      // Find similar clients.
      bool Added = false;
      for (auto &Group : Groups) {
        if (Group.tryAdd(CS, Hash)) {
          Added = true; break;
        }
      }

      if (!Added) {
        // we've not seen a client like this before.
        Groups.emplace_back(ServerConfig, Pool, CS, Hash);
      }

      info("Client has successfully registered.");

      UnregisteredSessions--;

    } else {
      // some other message?
      register_loop(CS);
    }
  });
}

} // namespace halo
