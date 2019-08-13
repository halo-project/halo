#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/ThreadPool.h"

#include "halo/ClientSession.h"
#include "halo/Compiler.h"
#include "halo/TaskQueueOverlay.h"

#include <functional>
#include <memory>


namespace halo {

  class ClientRegistrar;

// A group is a set of clients that are equal with respect to:
//
//  1. Bitcode
//  2. Target Triple
//  3. Other compilation flags.
//
class ClientGroup {
public:
  using ClientCollection = std::list<std::unique_ptr<ClientSession>>;

  std::atomic<size_t> NumActive;

  // Construct a singleton client group based on its initial member.
  ClientGroup(llvm::ThreadPool &Pool, ClientSession *CS)
      : NumActive(1), Pool(Pool), Queue(Pool) {

        if (!CS->Enrolled) {
          std::cerr << "was given a non-enrolled client!!\n";
          // TODO: proper error facilities.
        }

        // TODO: extract properties of this client
        pb::ClientEnroll &Client = CS->Client;
        pb::ModuleInfo const& Module = Client.module();

        // TODO: grab target triple, host cpu, and build flags.


        { // take ownership of the bitcode and move it into this group.
          std::string *BitcodeStr = Client.mutable_module()->release_bitcode();
          llvm::StringRef BitcodeRef(*BitcodeStr);
          Bitcode = std::move(llvm::MemoryBuffer::getMemBuffer(BitcodeRef));
          delete BitcodeStr;
        }

        addUnsafe(CS);
      }

  // returns true if the session became a member of the group.
  bool tryAdd(ClientSession *CS) {
    // can't determine anything until enrollment.
    if (!CS->Enrolled)
      return false;

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

  void testCompile() {
    Queue.async([&] () {

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
  std::unique_ptr<llvm::MemoryBuffer> Bitcode;

};

} // namespace halo
