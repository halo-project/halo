#pragma once

#include "llvm/Support/ThreadPool.h"

#include "halo/ClientSession.h"

#include <cinttypes>
#include <list>
#include <iostream>

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
  // returns the number of sessions removed.
  void cleanup() {
    auto it = Sessions.begin();
    size_t removed = 0;
    while(it != Sessions.end()) {
      if (it->Status == Dead)
        { it = Sessions.erase(it); removed++; }
      else
        it++;
    }
    assert(removed <= ActiveSessions);
    ActiveSessions -= removed;
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
      for (ClientSession &CS : Sessions)
        if (CS.Status == Active)
          CS.Queue.async( [&CS,Callable](){ Callable(CS); } );
  }

private:
  bool NoPersist;
  llvm::ThreadPool &Pool;
  uint32_t Port;
  asio::io_service &IOService;
  ip::tcp::endpoint Endpoint;
  ip::tcp::acceptor Acceptor;

  // these fields must only be accessed by the IOService's thread.
  std::list<ClientSession> Sessions;
  size_t ActiveSessions = 0;
  size_t TotalSessions = 0;

  void accept_loop() {
    Sessions.emplace_back(IOService, Pool);
    auto &Session = Sessions.back();
    // NOTE: can't capture 'Session' in the lambda,
    // so currently I'm assuming we never call accept_loop
    // non-recursively more than once!
    // Garbage collection should be okay though?

    Acceptor.async_accept(Session.Socket,
      [&](boost::system::error_code Err) {
        if(!Err) {
          std::cerr << "Accepted a new connection on port " << Port << "\n";
          TotalSessions++; ActiveSessions++;
          Sessions.back().start();
        }
        accept_loop();
      });
  }
}; // end class ClientRegistrar

}
