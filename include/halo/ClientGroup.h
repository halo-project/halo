#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/ThreadPool.h"
#include "llvm/Support/raw_ostream.h"

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
//  3. Data Layout
//  4. Host CPU  ??? Not sure if we want to be this discriminatory.
//  5. Other compilation flags.
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

        // TODO: grab Data Layout, host cpu, and build flags.
        TargetTriple = llvm::Triple(Client.process_triple());


        // take ownership of the bitcode, and maintain a MemoryBuffer view of it.
        BitcodeStorage = std::move(
            std::unique_ptr<std::string>(Client.mutable_module()->release_bitcode()));
        Bitcode = std::move(
            llvm::MemoryBuffer::getMemBuffer(llvm::StringRef(*BitcodeStorage)));

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
    static bool Done = false;

    if (Done)
      return;

    Done = true;

    Queue.async([&] () {

      orc::JITTargetMachineBuilder JTMB(TargetTriple);

      // FIXME: not only is this a slow call, it's also not correct!
      auto DL = JTMB.getDefaultDataLayoutForTarget();
      if (!DL) {
        llvm::errs() << "Error compiling module: "
                     << DL.takeError() << "\n";
        return;
      }

      Compiler Compile(JTMB, std::move(*DL));

      auto MaybeModule = llvm::getLazyBitcodeModule(Bitcode->getMemBufferRef(),
                                                Compile.getContext());
      if (!MaybeModule) {
        llvm::errs() << "Error compiling module: "
                     << MaybeModule.takeError() << "\n";
        return;
      }

      std::unique_ptr<llvm::Module> Module = std::move(MaybeModule.get());

      Module->print(llvm::errs(), nullptr);

      // This does not kick off a compilation. You need to request a symbol.
      Compile.addModule(std::move(Module));

      llvm::outs() << "triple = " << TargetTriple.str() << ". hit end of testcompile.\n";

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
  std::unique_ptr<std::string> BitcodeStorage;
  std::unique_ptr<llvm::MemoryBuffer> Bitcode;

  llvm::Triple TargetTriple;

};

} // namespace halo
