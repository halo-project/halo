#pragma once

#include "llvm/Support/ThreadPool.h"

#include "halo/ClientGroup.h"

#include <cinttypes>
#include <list>
#include <iostream>
#include <memory>
#include <functional>

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace msg = halo::msg;

namespace halo {

class ClientRegistrar {
public:
  ClientRegistrar(asio::io_service &service, uint32_t port, llvm::ThreadPool &TPool, bool nopersist)
      : NoPersist(nopersist),
        Pool(TPool),
        Port(port),
        IOService(service),
        Endpoint(ip::tcp::v4(), Port),
        Acceptor(IOService, Endpoint) {
          accept_loop();
        }

  // only safe to call within the IOService. Use io_service::dispatch.
  void cleanup() {
    for (auto &Group : Groups) {
      size_t removed = Group.cleanup();
      assert(removed <= ActiveSessions);
      ActiveSessions -= removed;
    }
  }

  bool consider_shutdown(bool ForcedShutdown) {
    if (ForcedShutdown ||
        (NoPersist && TotalSessions > 0 && ActiveSessions == 0)) {
      // TODO: a more graceful shutdown sould be nice. currently this
      // will abruptly send an RST packet to clients.
      IOService.stop();
      return true;
    }
    return false;
  }

  template <typename T>
  void run_service(T Callable) {
      // TODO: provide services to the groups!
      // for (ClientSession &CS : Sessions)
      //   if (CS.Status == Active)
      //     CS.Queue.async( [&CS,Callable](){ Callable(CS); } );
  }

private:
  bool NoPersist;
  llvm::ThreadPool &Pool;
  uint32_t Port;
  asio::io_service &IOService;
  ip::tcp::endpoint Endpoint;
  ip::tcp::acceptor Acceptor;

  // these fields must only be accessed by the IOService's thread.
  // TODO: Groups needs to be accessed in parallel. I don't want to have
  // everything through IOService thread.
  // We might need a TaskQueue here!
  
  std::list<ClientGroup> Groups;
  size_t ActiveSessions = 0;
  size_t TotalSessions = 0;

  void accept_loop() {
    // Not a unique_ptr because good luck moving one of those into the lambda!
    auto CS = new ClientSession(IOService, Pool);

    auto &Socket = CS->Socket;
    Acceptor.async_accept(Socket,
      [this,CS](boost::system::error_code Err) {
        if(!Err) {
          std::cerr << "Accepted a new connection on port " << Port << "\n";

          TotalSessions++; ActiveSessions++;
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
        CS->shutdown();
      } else if (Kind == msg::ClientEnroll) {
        std::string Blob(Body.data(), Body.size());

        CS->Client.ParseFromString(Blob);
        CS->Enrolled = true;

        msg::print_proto(CS->Client); // DEBUG

        if (Groups.empty()) {
          Groups.emplace_back(Pool);
        }

        // TODO: find the right group for this client.
        auto &Group = Groups.back();
        Group.add(CS);

      } else {
        // some other message?
        register_loop(CS);
      }
    });
  }
}; // end class ClientRegistrar

}
