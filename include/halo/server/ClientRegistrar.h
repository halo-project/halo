#pragma once

#include "halo/server/ThreadPool.h"
#include "halo/nlohmann/util.hpp"
#include "boost/asio.hpp"

#include <list>

namespace asio = boost::asio;
namespace ip = boost::asio::ip;

namespace halo {

class ClientSession;
class ClientGroup;

// there should only be one registrar instance per port (and thus server).
class ClientRegistrar {
public:
  ClientRegistrar(asio::io_service &service, nlohmann::json config);

  // only safe to call within the IOService. Use io_service::dispatch.
  // Because the IOService thread can modify the Groups list.
  void cleanup();

  bool consider_shutdown(bool ForcedShutdown);

  template <typename T>
  void apply(T Callable) {
    for (auto &Group : Groups)
      Callable(Group);
  }

private:
  bool NoPersist;
  uint32_t Port;
  nlohmann::json ServerConfig;
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

  void accept_loop();

  void register_loop(ClientSession *CS);

}; // end class ClientRegistrar

}