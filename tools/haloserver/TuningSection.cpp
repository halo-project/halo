#include "halo/tuner/TuningSection.h"
#include "halo/server/ClientGroup.h"
#include "halo/compiler/ReadELF.h"
#include "halo/tuner/NamedKnobs.h"
#include "halo/nlohmann/util.hpp"

namespace halo {

void AggressiveTuningSection::take_step(GroupState &State) {
  Steps++;

  /////////////////////////// BAKEOFF
  if (Status == ActivityState::TestingNewLib) {
    if (!Bakery.hasValue())
      fatal_error("no bakery available to conduct bakeoff");

    auto &Bakeoff = Bakery.getValue();

    switch (Bakeoff.take_step(State)) {
      case Bakeoff::Result::InProgress:
        return;

      case Bakeoff::Result::Finished: {
        auto NewBest = Bakeoff.getWinner();
        if (!NewBest)
          fatal_error("bakeoff successfully finished but no winner?");
        BestLib = NewBest.getValue();
        Status = ActivityState::Ready;
      }; return;

      case Bakeoff::Result::Timeout: {
        // the two libraries are too similar. we'll merge them.
        BakeoffTimeouts++;
        assert(Bakeoff.getDeployed() != Bakeoff.getOther());

        // we'll keep the currently deployed version, to avoid
        // unnessecary code switching.
        BestLib = Bakeoff.getDeployed();

        // merge and then remove the other version
        auto Other = Bakeoff.getOther();
        Versions[BestLib].forceMerge(Versions[Other]);
        Versions.erase(Other);

        Status = ActivityState::Ready;
      } return;
    };
    fatal_error("unhandled bakeoff case");
  }

  /////////////
  // if any clients just (re)joined, get them up-to-speed!
  assert(Versions.find(BestLib) != Versions.end() && "current version not already in database?");
  sendLib(State, Versions[BestLib]);
  redirectTo(State, Versions[BestLib]);

  // determine whether _any_ actions should be taken
  GroupPerf Perf = Profile.currentPerf(FnGroup, BestLib);

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
  Versions[BestLib].observeIPC(Perf.IPC);



  //////////////////////////// COMPILING
  if (Status == ActivityState::WaitingForCompile) {
    auto CompileDone = Compiler.dequeueCompilation();
    if (!CompileDone)
      return;

    CodeVersion NewCV {std::move(CompileDone.getValue())};

    // check if this is a duplicate
    bool Dupe = false;
    for (auto const& Entries : Versions)
      if ( (Dupe = Versions[Entries.first].tryMerge(NewCV)) )
        break;

    if (Dupe) {
      clogs() << "compile job produced duplicate code.\ntrying another compile!";
      DuplicateCompiles++;
      goto retryExperiment;
    }

    std::string NewLib = NewCV.getLibraryName();
    Versions[NewLib] = std::move(NewCV);

    Bakery = Bakeoff(State, this, BestLib, NewLib);

    Status = ActivityState::TestingNewLib;
    return;
  }



  ///////////////////////////////// READY

  // not going to take an experiment for now.
  if (ExploitSteps > 0) {
    ExploitSteps--;
    return;
  }

  // experiment!
  Experiments += 1;
retryExperiment:
  KnobSet NewKnobs = std::move(RandomTuner::randomFrom(BaseKnobs, gen));
  NewKnobs.dump();
  Compiler.enqueueCompilation(*Bitcode, std::move(NewKnobs));
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
          << "\n\tBestLib = " << BestLib
          << "\n\t# Steps = " << Steps
          << "\n\t# Experiments = " << Experiments
          << "\n\t# Bakeoff Timeouts = " << BakeoffTimeouts
          << "\n\tDuplicateCompiles = " << DuplicateCompiles
          << "\n\tSuccess Rate = " << SuccessRate << "%"
          << "\n";


  clogs() << "}\n\n";
}

AggressiveTuningSection::AggressiveTuningSection(TuningSectionInitializer TSI, std::string RootFunc)
  : TuningSection(TSI, RootFunc), gen(config::getServerSetting<uint64_t>("seed", TSI.Config)) {
    // create a version for the original library to record its performance, etc.
    CodeVersion OriginalLib{OriginalLibKnobs};
    std::string Name = OriginalLib.getLibraryName();
    Versions[Name] = std::move(OriginalLib);
    BestLib = Name;
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

  for (auto &Client : State.Clients) {
    // raise an error if the client doesn't already have this dylib!
    if (Client->State.DeployedLibs.count(LibName) == 0)
      fatal_error("trying to redirect client to library it doesn't already have!");

    // this client is already using the right lib.
    if (Client->State.CurrentLib == LibName)
      continue;

    clogs() << "redirecting " << FuncName << " to " << LibName << "\n";

    auto MaybeDef = Client->State.CRI.lookup(CodeRegionInfo::OriginalLib, FuncName);
    if (!MaybeDef) {
      warning("client is missing function definition for an original lib function: " + FuncName);
      continue;
    }
    auto OriginalDef = MaybeDef.getValue();

    pb::ModifyFunction MF;
    MF.set_name(FuncName);
    MF.set_addr(OriginalDef.Start);
    MF.set_desired_state(pb::FunctionState::REDIRECTED);
    MF.set_other_lib(LibName);
    MF.set_other_name(FuncName);

    Client->Chan.send_proto(msg::ModifyFunction, MF);
    Client->State.CurrentLib = LibName;
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
  KnobSet::InitializeKnobs(TSI.Config, BaseKnobs, MaxLoopID);

  ///////
  // Now, we initialize the OriginalLibKnobs as a subset of the BaseKnobs
  // We can only do this if the original build setting is in range of
  // the knob specified in the JSON file.

  auto OrigOptLvl = TSI.OriginalSettings.OptLvl;
  auto OK = std::make_unique<OptLvlKnob>(BaseKnobs.lookup<OptLvlKnob>(named_knob::OptimizeLevel));
  if (OK->getMin() <= OrigOptLvl && OrigOptLvl <= OK->getMax()) {
    OK->setVal(OrigOptLvl);
    OriginalLibKnobs.insert(std::move(OK));
  }

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