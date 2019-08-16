#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Triple.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#include "halo/ClientSession.h"
#include "halo/CompilationPipeline.h"
#include "halo/TaskQueueOverlay.h"
#include "halo/ThreadPool.h"

#include <functional>
#include <memory>
#include <list>
#include <utility>
#include <queue>


namespace halo {

  class ClientRegistrar;

// A group is a set of clients that are equal with respect to:
//
//  1. Bitcode
//  2. Target Triple
//  3. Data Layout
//  4. Host CPU  ??? Not sure if we want to be this discriminatory.
//  5. Other compilation flags.
//
class ClientGroup {
public:
  using ClientCollection = std::list<std::unique_ptr<ClientSession>>;

  std::atomic<size_t> NumActive;

  // Construct a singleton client group based on its initial member.
  ClientGroup(ThreadPool &Pool, ClientSession *CS)
      : NumActive(1), Pool(Pool), Queue(Pool) {

        if (!CS->Enrolled) {
          std::cerr << "was given a non-enrolled client!!\n";
          // TODO: proper error facilities.
        }

        // TODO: extract properties of this client
        pb::ClientEnroll &Client = CS->Client;
        pb::ModuleInfo const& Module = Client.module();

        // TODO: grab host cpu, and build flags.
        Pipeline = CompilationPipeline(llvm::Triple(Client.process_triple()));


        // take ownership of the bitcode, and maintain a MemoryBuffer view of it.
        BitcodeStorage = std::move(
            std::unique_ptr<std::string>(Client.mutable_module()->release_bitcode()));
        Bitcode = std::move(
            llvm::MemoryBuffer::getMemBuffer(llvm::StringRef(*BitcodeStorage))
                           );

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
      InFlight.push_back(Pool.asyncRet([&] () -> CompilationPipeline::compile_expected {

        auto Result = Pipeline.run(*Bitcode);

        llvm::outs() << "Finished Pipeline!\n";

        return Result;
      }));
    });
  }

  // ASYNC Apply the given callable to the entire collection of clients.
  template <typename RetTy>
  std::future<RetTy> apply(std::function<RetTy(ClientCollection&)> Callable) {
    return Queue.async([this,Callable] () {
              return Callable(Clients);
            });
  }

  // ASYNC Apply the callable to each client.
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


  ThreadPool &Pool;
  CompilationPipeline Pipeline;
  std::unique_ptr<std::string> BitcodeStorage;
  std::unique_ptr<llvm::MemoryBuffer> Bitcode;

  // The task queue provides sequential access to the fields below it.
  // The danger with locks when using a TaskPool is that if a task ever
  // blocks on a lock, that thread is stuck. There's no ability to yield / preempt
  // since there's no scheduler, so we lose threads this way.
  llvm::TaskQueueOverlay Queue;
  ClientCollection Clients;
  std::list<std::future<CompilationPipeline::compile_expected>> InFlight;
  /////////////////////////////////



};

} // namespace halo
