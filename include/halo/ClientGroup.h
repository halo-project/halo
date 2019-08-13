#pragma once

#include "llvm/Support/ThreadPool.h"
#include "halo/TaskQueueOverlay.h"

#include "halo/ClientSession.h"

#include <memory>
#include <functional>

namespace halo {

  class ClientRegistrar;

// A group is a set of clients that are equal with respect to:
//
//  1. Bitcode
//  2. Target Triple & other compilation flags.
//
class ClientGroup {
public:
  using ClientCollection = std::list<std::unique_ptr<ClientSession>>;

  std::atomic<size_t> NumActive;

  ClientGroup(llvm::ThreadPool &Pool) : NumActive(0), Pool(Pool), Queue(Pool) {}

  void add(ClientSession *CS) {
    NumActive++; // do this in the caller's thread eagarly.
    Queue.async([this,CS] () {
      CS->start(this);
      Clients.push_back(std::unique_ptr<ClientSession>(CS));
    });
  }

  void cleanup() {
    Queue.async([&] () {
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

  // Apply the given callable to the entire collection of clients.
  template <typename RetTy>
  std::future<RetTy> operator () (std::function<RetTy(ClientCollection&)> Callable) {
    return Queue.async([this,Callable] () {
              return Callable(Clients);
            });
  }

  // Apply the callable to each client.
  std::future<void> apply(std::function<void(ClientSession&)> Callable) {
    return Queue.async([this,Callable] () {
              for (auto &Client : Clients)
                Callable(*Client);
          });
  }

private:
  // the queue provides sequentially consistent access the the members below it.
  llvm::ThreadPool &Pool;
  llvm::TaskQueueOverlay Queue;
  ClientCollection Clients;

};

} // namespace halo
