
#include "halo/compiler/ReadELF.h"
#include "halo/server/ClientGroup.h"
#include "halo/tuner/Utility.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Triple.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/raw_ostream.h"

#include "Logging.h"

namespace halo {

  // llvm::Error translateSymbols(CodeRegionInfo const& CRI, pb::CodeReplacement &CR) {
  //   // The translation here is to fill in the function addresses
  //   // for a specific memory layout. Note that this mutates CR,
  //   // but for fields we expect to be overwritten.

  //   proto::RepeatedPtrField<pb::FunctionSymbol> *Syms = CR.mutable_symbols();
  //   for (pb::FunctionSymbol &FSym : *Syms) {
  //     auto FI = CRI.lookup(FSym.label());

  //     if (FI == nullptr)
  //       return makeError("unable to find function addr for this client.");

  //     auto const& Defs = FI->getDefinitions();

  //     if (Defs.size() != 1)
  //       return makeError("expected exactly one definition of the function.");

  //     auto& Def = Defs[0];
  //     if (!Def.Patchable)
  //       return makeError("single function definition expected to be patchable!");

  //     FSym.set_addr(Def.Start);
  //   }

  //   return llvm::Error::success();
  // }

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

    std::this_thread::sleep_for(std::chrono::milliseconds(ServiceIterationRate));
    run_service_loop();
  }


  void ClientGroup::run_service_loop() {
    withState([this] (GroupState &State) {

      // Update the profiler with new PerfData, if any.
      Profile.consumePerfData(State.Clients);

      Profile.decay();

      auto MaybeInfo = Profile.getBestTuningSection();

      if (!MaybeInfo) {
        logs() << "No sample data available.\n";
        return end_service_iteration();
      }

      auto &Info = MaybeInfo.getValue();
      std::string HotFuncName = Info.first;
      bool Patchable = Info.second;
      bool HaveBitcode = Profile.haveBitcode(HotFuncName);

      logs() << "Hottest function = " << HotFuncName << "\n";

      if (!Patchable || !HaveBitcode) {
        logs() << "    <patchable = " << Patchable << ", bitcode = " << HaveBitcode << ">\n";
        return end_service_iteration();
      }

      // 2. send a request to client to begin timing the execution of the function.
      // for (auto &Client : State.Clients) {
      //   if (Client->Status == SessionStatus::Measuring)
      //     continue;

      //   pb::FunctionAddress FA;
      //   auto FI = Client->State.CRI.lookup(FuncName);
      //   FA.set_func_addr(FI->getStart());
      //   Client->Chan.send_proto(msg::StartMeasureFunction, FA);
      //   Client->Status = SessionStatus::Measuring;
      // }
      // return end_service_iteration(); // FIXME: temporary

      // FIXME: For now we're assuming only one compile configuration, so we just
      // check to see if we should queue a new job if a client needs it.
      // It's still wasteful because we don't check if there's an equivalent
      // one already in flight, but whatever.
      bool ShouldCompile = false;
      for (auto &Client : State.Clients) {
        if (Client->State.DeployedCode.count(HotFuncName))
          continue;
        ShouldCompile = true;
        break;
      }

      // 3. queue up a new version. TODO: stop doing this blindly
      if (ShouldCompile) {
        Compiler.enqueueCompilation(*Bitcode, Info, Knobs);
      }

      // 4. check if any versions are done, and send the first one that's done
      //    to all clients.
      auto CodeResult = Compiler.dequeueCompilation();
      if (!CodeResult)
        return end_service_iteration(); // nothing's ready right now.

      auto CompileOut = std::move(CodeResult.getValue());

      auto MaybeBuf = std::move(CompileOut.Result);
      if (!MaybeBuf)
        return end_service_iteration(); // an error etc happened and was logged elsewhere

      std::unique_ptr<llvm::MemoryBuffer> Buf = std::move(MaybeBuf.getValue());
      std::string LibName = CompileOut.UniqueJobName;
      std::string FuncName = CompileOut.TS.first;

      // tell all clients to load this object file into memory.
      pb::LoadDyLib DylibMsg;
      DylibMsg.set_name(LibName);
      DylibMsg.set_objfile(Buf->getBufferStart(), Buf->getBufferSize());

      // Find all function symbols in the dylib
      auto ELFReadError = readSymbolInfo(Buf->getMemBufferRef(), DylibMsg, FuncName);
      if (ELFReadError)
        fatal_error(std::move(ELFReadError));

      // For now. send to all clients who don't already have a JIT'd version
      for (auto &Client : State.Clients) {

        if (Client->State.DeployedCode.count(FuncName))
          continue;

        auto MaybeDef = Client->State.CRI.lookup(CodeRegionInfo::OriginalLib, FuncName);
        if (!MaybeDef)
          fatal_error("client is missing CRI info for an original lib function: " + FuncName);
        auto OriginalDef = MaybeDef.getValue();

        pb::ModifyFunction MF;
        MF.set_name(FuncName);
        MF.set_addr(OriginalDef.Start);
        MF.set_desired_state(pb::FunctionState::REDIRECTED);
        MF.set_other_lib(LibName);
        MF.set_other_name(FuncName);

        // auto Error = translateSymbols(Client->State.CRI, CodeMsg);
        // if (Error)
        //   logs() << "Error translating symbols: " << Error << "\n";

        Client->Chan.send_proto(msg::LoadDyLib, DylibMsg);
        Client->Chan.send_proto(msg::ModifyFunction, MF);

        Client->State.DeployedCode.insert(FuncName);
      }

      logs() << "Sent code to all clients!\n";

      return end_service_iteration();
    }); // end of lambda
  }


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

bool ClientGroup::tryAdd(ClientSession *CS, std::array<uint8_t, 20> &TheirHash) {
  assert(CS->Enrolled);

  if (BitcodeHash != TheirHash)
    return false;

  pb::ClientEnroll &CE = CS->Client;
  if (!Pipeline.getTriple().isCompatibleWith(llvm::Triple(CE.process_triple())))
    return false;

  if (Pipeline.getCPUName() != CE.host_cpu())
    return false;

  NumActive++; // do this in the caller's thread eagarly.
  withState([this,CS] (GroupState &State) {
    addSession(this, CS, State);
  });

  return true;
}


ClientGroup::ClientGroup(JSON const& Config, ThreadPool &Pool, ClientSession *CS, std::array<uint8_t, 20> &BitcodeSHA1)
    : SequentialAccess(Pool), NumActive(1), ServiceLoopActive(false),
      ShouldStop(false), ServiceIterationRate(250), Pool(Pool), Compiler(Pool), BitcodeHash(BitcodeSHA1) {

      KnobSet::InitializeKnobs(Config, Knobs);

      if (!CS->Enrolled)
        llvm::report_fatal_error("was given a non-enrolled client!");

      pb::ClientEnroll &Client = CS->Client;

      // TODO: parse the build flags and perhaps have client
      // send llvm::sys::getHostCPUFeatures() info for -mattr flags.
      Pipeline = CompilationPipeline(
                    llvm::Triple(Client.process_triple()),
                    Client.host_cpu());

      Compiler.setCompilationPipeline(&Pipeline);


      // take ownership of the bitcode, and maintain a MemoryBuffer view of it.
      BitcodeStorage = std::move(
          std::unique_ptr<std::string>(Client.mutable_module()->release_bitcode()));
      Bitcode = std::move(
          llvm::MemoryBuffer::getMemBuffer(llvm::StringRef(*BitcodeStorage))
                         );

      Pipeline.analyzeForProfiling(Profile, *Bitcode);

      withState([this,CS] (GroupState &State) {
        addSession(this, CS, State);
      });
    }

} // namespace halo
