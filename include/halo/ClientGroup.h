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

  // Construct a singleton client group based on its initial member.
  ClientGroup(llvm::ThreadPool &Pool, ClientSession *CS)
      : NumActive(1), Pool(Pool), Queue(Pool) {
        // TODO: extract properties of this client
        addUnsafe(CS);
      }

  // returns true if the session became a member of the group.
  bool tryAdd(ClientSession *CS) {
    // TODO: actually check if the client is compatible with this group.

    NumActive++; // do this in the caller's thread eagarly.
    Queue.async([this,CS] () {
      addUnsafe(CS);
    });
    return true;
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

  void addUnsafe(ClientSession *CS) {
    CS->start(this);
    Clients.push_back(std::unique_ptr<ClientSession>(CS));
  }

  // the queue provides sequentially consistent access the the members below it.
  llvm::ThreadPool &Pool;
  llvm::TaskQueueOverlay Queue;
  ClientCollection Clients;

};

} // namespace halo
