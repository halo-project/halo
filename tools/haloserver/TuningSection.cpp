#include "halo/tuner/TuningSection.h"
#include "halo/server/ClientGroup.h"
#include "halo/compiler/ReadELF.h"

namespace halo {


void AggressiveTuningSection::take_step(GroupState &State) {
  Steps++;

  GroupPerf Perf = Profile.currentPerf(FnGroup, CurrentLib.getLibraryName());

  // if no new samples have hit any of the functions in the group since last time,
  // or we don't have a valid IPC, we do nothing.
  if (SamplesLastTime == Perf.SamplesSeen || Perf.IPC <= 0)
    return;

  clogs() << "\n--------\n"
          << "Group IPC = " << Perf.IPC
          << "\nGroup Hotness = " << Perf.Hotness
          << "\nGroup Samples = " << Perf.SamplesSeen
          << "\n--------\n";

  SamplesLastTime = Perf.SamplesSeen;
  CurrentLib.observeIPC(Perf.IPC);

  // this lib is too young to get a decent picture of what's going on.
  if (CurrentLib.recordedIPCs() < std::max(10UL, EXPLOIT_FACTOR / 2))
    return;

  if (Status == ActivityState::WaitingForCompile) {
    auto CompileDone = Compiler.dequeueCompilation();
    if (!CompileDone)
      return;

    CodeVersion NewLib(std::move(CompileDone.getValue()));

    sendLib(State, NewLib);
    redirectTo(State, NewLib);

    PrevLib = std::move(CurrentLib);
    CurrentLib = std::move(NewLib);

    clogs() << "sent JIT'd code for " << FnGroup.Root << "\n";
    Status = ActivityState::TestingNewLib;
    return;
  }

  if (Status == ActivityState::TestingNewLib) {
    auto Answer = PrevLib.betterThan(CurrentLib);

    if (!Answer.hasValue())
      return; // we will keep waiting for more samples to come in.

    if (Answer.getValue()) {
      // then prev is better, let's revert.
      redirectTo(State, PrevLib);
      CurrentLib = std::move(PrevLib);
    } else {
      // we're going to keep the current one!
      SuccessfulExperiments++;
    }

    ExploitSteps = EXPLOIT_FACTOR;
    Status = ActivityState::Ready;
    return;
  }

  // not going to take an experiment for now.
  if (ExploitSteps > 0) {
    ExploitSteps--;
    return;
  }

  // experiment!
  Experiments += 1;
  randomlyChange(Knobs, gen);
  Knobs.dump();
  Compiler.enqueueCompilation(*Bitcode, Knobs);
  Status = ActivityState::WaitingForCompile;
}


void AggressiveTuningSection::dump() const {
  clogs() << "TuningSection for " << FnGroup.Root << " {"\
          << "\n\tAllFuncs = ";

  for (auto const& Func : FnGroup.AllFuncs)
    clogs() << Func << ", ";

  float SuccessRate = Experiments == 0
                        ? 0
                        : 100.0 * (((float)SuccessfulExperiments) / ((float)Experiments));


  clogs() << "\n\tStatus = " << stateToString(Status)
          << "\n\tCurrentLib = " << CurrentLib.getLibraryName()
          << "\n\tPrevLib = " << PrevLib.getLibraryName()
          << "\n\t# Steps = " << Steps
          << "\n\t# Experiments = " << Experiments
          << "\n\tSuccess Rate = " << SuccessRate << "%"
          << "\n";


  clogs() << "}\n\n";
}

void TuningSection::sendLib(GroupState &State, CodeVersion const& CV) {
  if (CV.isBroken()) {
    warning("trying to send broken lib.");
    return;
  }

  if(CV.isOriginalLib())
    return; // nothing to do!

  std::unique_ptr<llvm::MemoryBuffer> const& Buf = CV.getObjectFile();
  std::string LibName = CV.getLibraryName();
  std::string FuncName = FnGroup.Root;

  // tell all clients to load this object file into memory.
  pb::LoadDyLib DylibMsg;
  DylibMsg.set_name(LibName);
  DylibMsg.set_objfile(Buf->getBufferStart(), Buf->getBufferSize());

  // Find all function symbols in the dylib
  auto ELFReadError = readSymbolInfo(Buf->getMemBufferRef(), DylibMsg, FuncName);
  if (ELFReadError)
    fatal_error(std::move(ELFReadError));

  for (auto &Client : State.Clients) {
    auto &DeployedLibs = Client->State.DeployedLibs;

    // send the dylib only if the client doesn't have it already
    if (DeployedLibs.count(LibName) == 0) {
      Client->Chan.send_proto(msg::LoadDyLib, DylibMsg);
      DeployedLibs.insert(LibName);
    }
  }
}

void TuningSection::redirectTo(GroupState &State, CodeVersion const& CV) {
  if (CV.isBroken()) {
    warning("trying to redirect to broken lib.");
    return;
  }

  std::string LibName = CV.getLibraryName();
  std::string FuncName = FnGroup.Root;

  clogs() << "redirecting " << FuncName << " to " << LibName << "\n";

  for (auto &Client : State.Clients) {
    // raise an error if the client doesn't already have this dylib!
    if (Client->State.DeployedLibs.count(LibName) == 0)
      fatal_error("trying to redirect client to library it doesn't already have!");

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

    Client->Chan.send_proto(msg::ModifyFunction, MF);
  }


}

TuningSection::TuningSection(TuningSectionInitializer TSI, std::string RootFunc)
    : FnGroup(RootFunc), Compiler(TSI.Pool, TSI.Pipeline), Profile(TSI.Profile) {
  ////////////
  // Choose the set of all funcs in this tuning section.

  // start off with all functions reachable according to the call-graph
  auto Reachable = Profile.getCallGraph().allReachable(RootFunc);

  // filter down that set to just those for which we have bitcode
  for (auto const& Func : Reachable)
    if (Profile.haveBitcode(Func))
      FnGroup.AllFuncs.insert(Func);

  // now, we clean-up the original bitcode to only include those functions
  auto MaybeResult = TSI.Pipeline.cleanup(TSI.OriginalBitcode, FnGroup.Root, FnGroup.AllFuncs);
  if (!MaybeResult)
    fatal_error("couldn't clean-up bitcode for tuning section!");

  auto Result = std::move(MaybeResult.getValue());
  Bitcode = std::move(Result.first);
  unsigned MaxLoopID = Result.second;

  /////
  // Finally, we can initialize the knobs for this tuning section
  KnobSet::InitializeKnobs(TSI.Config, Knobs, MaxLoopID);
}



llvm::Optional<std::unique_ptr<TuningSection>> TuningSection::Create(Strategy Strat, TuningSectionInitializer TSI) {
  auto MaybeHotNode = TSI.Profile.hottestNode();
  if (!MaybeHotNode) return llvm::None;

  auto MaybeAncestor = TSI.Profile.findSuitableTuningRoot(MaybeHotNode.getValue());
  if (!MaybeAncestor) return llvm::None;

  std::string PatchableAncestorName = MaybeAncestor.getValue();

  if (!TSI.Profile.haveBitcode(PatchableAncestorName))
    return llvm::None;

  TuningSection *TS = nullptr;
  switch (Strat) {
    case Strategy::Aggressive:
      TS = new AggressiveTuningSection(TSI, PatchableAncestorName);
      break;
  };

  if (TS == nullptr)
    fatal_error("unknown / unrecognized TuningSection strategy.");

  return std::unique_ptr<TuningSection>(TS);
}

} // end namespace