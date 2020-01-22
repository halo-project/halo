#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/SHA1.h"

#include "halo/server/ClientGroup.h"
#include "halo/server/ThreadPool.h"
#include "halo/nlohmann/json_fwd.hpp"

#include "Logging.h"

#include <cinttypes>
#include <list>
#include <memory>
#include <functional>

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace msg = halo::msg;

using JSON = nlohmann::json;

namespace halo {

class ClientRegistrar {
public:
  ClientRegistrar(asio::io_service &service, uint32_t port, bool nopersist, JSON config)
      : NoPersist(nopersist),
        Port(port),
        ServerConfig(config),
        IOService(service),
        Endpoint(ip::tcp::v4(), Port),
        Acceptor(IOService, Endpoint) {
          accept_loop();
        }

  // only safe to call within the IOService. Use io_service::dispatch.
  // Because the IOService thread can modify the Groups list.
  void cleanup() {
    for (auto &Group : Groups)
      Group.cleanup_async();
  }

  bool consider_shutdown(bool ForcedShutdown) {
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
          // std::cerr << "Group has " << Active << " active.\n";
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

  template <typename T>
  void apply(T Callable) {
    for (auto &Group : Groups)
      Callable(Group);
  }

private:
  bool NoPersist;
  uint32_t Port;
  JSON ServerConfig;
  asio::io_service &IOService;
  ip::tcp::endpoint Endpoint;
  ip::tcp::acceptor Acceptor;
  ThreadPool Pool;

  // these fields must only be accessed by the IOService's thread.
  // TODO: Groups needs to be accessed in parallel. I don't want to have
  // everything through IOService thread.
  // We might need a TaskQueue here!

  std::list<ClientGroup> Groups;
  size_t TotalSessions = 0;
  size_t UnregisteredSessions = 0;

  void accept_loop() {
    // Not a unique_ptr because good luck moving one of those into the lambda!
    auto CS = new ClientSession(IOService, Pool);

    auto &Socket = CS->Socket;
    Acceptor.async_accept(Socket,
      [this,CS](boost::system::error_code Err) {
        if(!Err) {
          info("Received a new connection request.", true);

          TotalSessions++; UnregisteredSessions++;
          CS->Status = Active;
          asio::socket_base::keep_alive option(true);
          CS->Socket.set_option(option);

          register_loop(CS);
        }
        accept_loop();
      });
  }

  void register_loop(ClientSession *CS) {
    // NOTE: It's only ok to access Groups here because we know the IOService
    // of the client registrar = IOService of all clients.
    auto &Chan = CS->Chan;
    Chan.async_recv([this,CS](msg::Kind Kind, std::vector<char>& Body) {
      if (Kind == msg::Shutdown) {
        // It never made it into a group, so we clean it up.
        CS->shutdown();
        delete CS;
        UnregisteredSessions--;

      } else if (Kind == msg::ClientEnroll) {
        llvm::StringRef Blob(Body.data(), Body.size());

        CS->Client.ParseFromString(Blob);
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

        info("Client has successfully registered.", true);

        UnregisteredSessions--;

      } else {
        // some other message?
        register_loop(CS);
      }
    });
  }
}; // end class ClientRegistrar

}