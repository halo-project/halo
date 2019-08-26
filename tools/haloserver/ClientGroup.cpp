
#include "halo/ClientGroup.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Triple.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/raw_ostream.h"

namespace halo {

  // kicks off a continuous service loop for this group.
  void ClientGroup::run_services() {
    if (!RunningServices)
      RunningServices = true;

    withState([this] (GroupState &State) {
      FunctionInfo *HottestFI = nullptr;

      // FIXME: this needs to be revised since the profile requires thread-safe
      // access.

      // for (auto &Client : State.Clients) {
      //   if (Client->Status != Measuring) {
      //     // 1. determine which function is the hottest for this session.
      //     FunctionInfo *FI = Client->Profile.getMostSampled();
      //     if (FI) {
      //       std::cout << "Hottest function = " << FI->Name << "\n";
      //       HottestFI = FI;
      //       break; // FIXME
      //     }
      //   }
      // }

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

      // TODO: sleep
      run_services();
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


void ClientGroup::cleanup_async() {
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


void addSession (ClientGroup *Group, ClientSession *CS, GroupState &State) {
  CS->start(Group);
  State.Clients.push_back(std::unique_ptr<ClientSession>(CS));
}

bool ClientGroup::tryAdd(ClientSession *CS) {
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


ClientGroup::ClientGroup(ThreadPool &Pool, ClientSession *CS)
    : SequentialAccess(Pool), NumActive(1), RunningServices(false), Pool(Pool) {

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

} // namespace halo
