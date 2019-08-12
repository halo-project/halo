#pragma once

#include "llvm/Support/ThreadPool.h"

#include "halo/ClientSession.h"

#include <mutex>
#include <memory>

namespace halo {

  class ClientRegistrar;

// A group is a set of clients that are equal with respect to:
//
//  1. Bitcode
//  2. Target Triple & other compilation flags.
//
class ClientGroup {
public:

  ClientGroup(llvm::ThreadPool &Pool) : Pool(Pool) {}

  void add(ClientSession *CS) {
    std::lock_guard<std::mutex> lock(ClientMutex);

    CS->start(this);
    Clients.push_back(std::unique_ptr<ClientSession>(CS));
  }

  // returns number of clients removed.
  size_t cleanup() {
    std::lock_guard<std::mutex> lock(ClientMutex);

    auto it = Clients.begin();
    size_t removed = 0;
    while(it != Clients.end()) {
      if ((*it)->Status == Dead)
        { it = Clients.erase(it); removed++; }
      else
        it++;
    }
    return removed;
  }

private:

  llvm::ThreadPool &Pool;

  std::mutex ClientMutex;
  std::list<std::unique_ptr<ClientSession>> Clients;

};

} // namespace halo
