
#include "halo/ClientGroup.h"
#include "halo/Utility.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Triple.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/raw_ostream.h"

#include "Logging.h"

namespace halo {

  llvm::Error translateSymbols(CodeRegionInfo const& CRI, pb::CodeReplacement &CR) {
    // The translation here is to fill in the function addresses
    // for a specific memory layout. Note that this mutates CR,
    // but for fields we expect to be overwritten.

    proto::RepeatedPtrField<pb::FunctionSymbol> *Syms = CR.mutable_symbols();
    for (pb::FunctionSymbol &FSym : *Syms) {
      auto *FI = CRI.lookup(FSym.label());

      if (FI == nullptr)
        return llvm::createStringError(std::errc::not_supported,
            "unable to find function addr for this client.");

      FSym.set_addr(FI->AbsAddr);
    }

    return llvm::Error::success();
  }

  // kicks off a continuous service loop for this group.
  void ClientGroup::start_services() {
    if (ServiceLoopActive)
      return;

    ServiceLoopActive = true;
    run_service_loop();
  }

  void ClientGroup::end_service_iteration() {
    // This method should be called before the service loop function ends
    // to queue up another iteration, otherwise we will be stalled forever.

    if (ShouldStop) {
      ServiceLoopActive = false;
      return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    run_service_loop();
  }

  void ClientGroup::run_service_loop() {
    withState([this] (GroupState &State) {
      auto MaybeName = Profile.getMostSampled(State.Clients);

      if (!MaybeName) {
        llvm::outs() << "No most sampled function\n";
        return end_service_iteration();
      }

      llvm::StringRef Name = MaybeName.getValue();
      llvm::outs() << "Hottest function = " << Name << "\n";



      // 2. send a request to client to begin timing the execution of the function.
      // CS.Measuring = true;
      // pb::ReqMeasureFunction MF;
      // MF.set_func_addr(FI->AbsAddr);
      // CS.Chan.send_proto(msg::ReqMeasureFunction, MF);

      // For now we're assuming only one compile configuration, so we just
      // check to see if we should queue a new job if a client needs it.
      // It's still wasteful because we don't check if there's an equivalent
      // one already in flight, but whatever. FIXME
      bool ShouldCompile = false;
      for (auto &Client : State.Clients) {
        if (Client->State.DeployedCode.count(Name))
          continue;
        ShouldCompile = true;
        break;
      }

      // 3. queue up a new version. TODO: stop doing this blindly
      if (ShouldCompile) {
        State.InFlight.emplace_back(Name,
          std::move(Pool.asyncRet([this,Name] () -> CompilationPipeline::compile_expected {

          auto Result = Pipeline.run(*Bitcode, Name);

          llvm::outs() << "Finished Compile!\n";

          return Result;
        })));
      }

      // 4. check if any versions are done, and send the first one that's done
      //    to all clients.

      // TODO: this needs to also remove the future from the list!
      for (auto &Pair : State.InFlight) {
        auto &Name = Pair.first;
        auto &Future = Pair.second;

        if (Future.valid() && get_status(Future) == std::future_status::ready) {
          auto MaybeBuf = std::move(Future.get());

          if (!MaybeBuf)
            continue; // an error etc happened and was logged elsewhere

          std::unique_ptr<llvm::MemoryBuffer> Buf = std::move(MaybeBuf.getValue());

          pb::CodeReplacement CodeMsg;
          CodeMsg.set_objfile(Buf->getBufferStart(), Buf->getBufferSize());

          // Add the function symbols we request to replace on the client with
          // the ones contained in the object file.
          pb::FunctionSymbol *FS = CodeMsg.add_symbols();
          FS->set_label(Name);

          // For now. send to all clients who don't already have a JIT'd version
          for (auto &Client : State.Clients) {

            if (Client->State.DeployedCode.count(Name))
              continue;

            auto Error = translateSymbols(Client->State.Data.CRI, CodeMsg);

            if (Error)
              log() << "Error translating symbols: " << Error << "\n";

            Client->Chan.send_proto(msg::CodeReplacement, CodeMsg);
            Client->State.DeployedCode.insert(Name);
          }

          llvm::outs() << "Sent code to all clients!\n";

          break;
        }
      }

      return end_service_iteration();
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
    : SequentialAccess(Pool), NumActive(1), Pool(Pool),
      ServiceLoopActive(false), ShouldStop(false) {

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
