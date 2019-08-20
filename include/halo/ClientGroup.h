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


struct GroupState {
  using ClientCollection = std::list<std::unique_ptr<ClientSession>>;

  ClientCollection Clients;
  std::list<std::pair<std::string,
                      std::future<CompilationPipeline::compile_expected>
                     >
           > InFlight;
};

// Manages members of a ClientGroup that require locking to ensure thread safety.
class ClientGroupBase {
public:
  ClientGroupBase(ThreadPool &Pool) : Queue(Pool) {}

  // ASYNC Apply the given callable to the state. Provides sequential and
  // non-overlapping access to the group's state.
  template <typename RetTy>
  std::future<RetTy> withState(std::function<RetTy(GroupState&)> Callable) {
    return Queue.async([this,Callable] () {
              return Callable(State);
            });
  }

  std::future<void> withState(std::function<void(GroupState&)> Callable) {
    return Queue.async([this,Callable] () {
              Callable(State);
            });
  }

  std::future<void> eachClient(std::function<void(ClientSession&)> Callable) {
    return withState([Callable] (GroupState& State) {
      for (auto &Client : State.Clients)
        Callable(*Client);
    });
  }

private:
  // The task queue provides sequential access to the group's state.
  // The danger with locks when using a TaskPool is that if a task ever
  // blocks on a lock, that thread is stuck. There's no ability to yield / preempt
  // since there's no scheduler, so we lose threads this way.
  llvm::TaskQueueOverlay Queue;
  GroupState State;
  /////////////////////////////////
}; // end class

void addSession (ClientGroup *Group, ClientSession *CS, GroupState &State) {
  CS->start(Group);
  State.Clients.push_back(std::unique_ptr<ClientSession>(CS));
}

// A group is a set of clients that are equal with respect to:
//
//  1. Bitcode
//  2. Target Triple
//  3. Data Layout
//  4. Host CPU  ??? Not sure if we want to be this discriminatory.
//  5. Other compilation flags.
//
class ClientGroup : public ClientGroupBase {
public:
  std::atomic<size_t> NumActive;

  // Construct a singleton client group based on its initial member.
  ClientGroup(ThreadPool &Pool, ClientSession *CS)
      : ClientGroupBase(Pool), NumActive(1), Pool(Pool) {

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

        withState([this,CS] (GroupState &State) {
          addSession(this, CS, State);
        });
      }

  // returns true if the session became a member of the group.
  bool tryAdd(ClientSession *CS) {
    // can't determine anything until enrollment.
    if (!CS->Enrolled)
      return false;

    // TODO: actually check if the client is compatible with this group.

    NumActive++; // do this in the caller's thread eagarly.
    withState([this,CS] (GroupState &State) {
      addSession(this, CS, State);
    });

    return true;
  }

  void cleanup() {
    withState([&] (GroupState &State) {
      auto &Clients = State.Clients;
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

  void service_async() {
    withState([this] (GroupState &State) {
      FunctionInfo *HottestFI = nullptr;
      for (auto &Client : State.Clients) {
        if (!Client->Measuring) {
          // 1. determine which function is the hottest for this session.
          FunctionInfo *FI = Client->Profile.getMostSampled();
          if (FI) {
            std::cout << "Hottest function = " << FI->Name << "\n";
            HottestFI = FI;
            break; // FIXME
          }
        }
      }

      if (HottestFI == nullptr)
        return;



      // 2. send a request to client to begin timing the execution of the function.
      // CS.Measuring = true;
      // pb::ReqMeasureFunction MF;
      // MF.set_func_addr(FI->AbsAddr);
      // CS.Chan.send_proto(msg::ReqMeasureFunction, MF);

      // 3. queue up a new version.
      State.InFlight.emplace_back(HottestFI->Name,
        std::move(Pool.asyncRet([&] () -> CompilationPipeline::compile_expected {

        auto Result = Pipeline.run(*Bitcode);

        llvm::outs() << "Finished Compile!\n";

        return Result;
      })));

      // 4. check if any versions are done, and send the first one that's done
      //    to all clients.

      // TODO: this needs to also remove the future from the list!
      for (auto &Pair : State.InFlight) {
        auto &Name = Pair.first;
        auto &Future = Pair.second;

        if (Future.valid() && get_status(Future) == std::future_status::ready) {
          auto MaybeBuf = std::move(Future.get());

          if (!MaybeBuf) {
            llvm::outs() << "Error during compilation: "
              << MaybeBuf.takeError() << "\n";
          }

          std::unique_ptr<llvm::MemoryBuffer> Buf = std::move(MaybeBuf.get());

          pb::CodeReplacement CodeMsg;
          CodeMsg.set_objfile(Buf->getBufferStart(), Buf->getBufferSize());

          // Add the function symbols we request to replace on the client with
          // the ones contained in the object file.
          pb::FunctionSymbol *FS = CodeMsg.add_symbols();
          FS->set_label(Name);

          // For now. send to all clients :)
          for (auto &Client : State.Clients) {
            Client->translateSymbols(CodeMsg);
            Client->Chan.send_proto(msg::CodeReplacement, CodeMsg);
          }

          llvm::outs() << "Sent code to all clients!\n";

          break;
        }
      }

    }); // end of lambda
  }

  /*
  void service_session(ClientSession &CS) {

  }
  */

  // void compileTest() {
  //   static bool Compiled = false;
  //   static std::atomic<bool> Sent(false);
  //
  //   if (!Compiled) {
  //     Compiled = true;
  //     withState([&] (GroupState &State) {
  //       State.InFlight.push_back(Pool.asyncRet([&] () -> CompilationPipeline::compile_expected {
  //
  //         auto Result = Pipeline.run(*Bitcode);
  //
  //         llvm::outs() << "Finished Compile!\n";
  //
  //         return Result;
  //       }));
  //     });
  //   }
  //
  //   if (!Sent) {
  //     // TODO: this should also remove the future from the list.
  //     withState([&] (GroupState &State) {
  //
  //     });
  //   }
  //
  // }

private:

  ThreadPool &Pool;
  CompilationPipeline Pipeline;
  std::unique_ptr<std::string> BitcodeStorage;
  std::unique_ptr<llvm::MemoryBuffer> Bitcode;

};

} // namespace halo
