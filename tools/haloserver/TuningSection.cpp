#include "halo/tuner/TuningSection.h"
#include "halo/server/ClientGroup.h"
#include "halo/compiler/ReadELF.h"
#include "halo/tuner/NamedKnobs.h"
#include "halo/nlohmann/util.hpp"

namespace halo {

void AggressiveTuningSection::transitionTo(ActivityState S) {
  ActivityState From = Status;
  ActivityState To = S;

  // self-loop?
  if (From == To)
    return;
  else
    Status = To;  // we actually changed to a different state.
}

/// The implementation of this take-step always must assume that
/// a client has joined at an arbitrary time. So actions such as
/// enabling/disabling sampling, or sending the current code, should be
/// attempted on every step to make sure they're in the right state.
void AggressiveTuningSection::take_step(GroupState &State) {
  Steps++;

  /////////////////////////// BAKEOFF
  if (Status == ActivityState::TestingNewLib) {
    // make sure all clients are sampling right now
    ClientGroup::broadcastSamplingPeriod(State, Profile.getSamplePeriod());

    // update CCT etc
    Profile.consumePerfData(State);

    assert(Bakery.hasValue() && "no bakery when trying to test a lib?");
    auto &Bakeoff = Bakery.getValue();

    switch (Bakeoff.take_step(State)) {
      case Bakeoff::Result::InProgress:
        return transitionTo(ActivityState::TestingNewLib);

      case Bakeoff::Result::Finished: {
        auto NewBest = Bakeoff.getWinner();
        if (!NewBest)
          fatal_error("bakeoff successfully finished but no winner?");
        BestLib = NewBest.getValue();
        return transitionTo(ActivityState::Paused);
      };

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

        return transitionTo(ActivityState::Paused);
      };
    };
    fatal_error("unhandled bakeoff case");
  }



  /////////////
  // Since we're not in a bakeoff, if any clients just
  // (re)joined, get them up-to-speed with the best one!
  //
  assert(Versions.find(BestLib) != Versions.end() && "current version not already in database?");
  sendLib(State, Versions[BestLib]);
  redirectTo(State, Versions[BestLib]);

  // make sure all clients are not sampling right now
  ClientGroup::broadcastSamplingPeriod(State, 0);



  //////////////////////////// PAUSED / EXPLOITNG
  if (Status == ActivityState::Paused) {

    if (ExploitSteps > 0) {
      // not going to take an experiment for now.
      ExploitSteps--;
      return transitionTo(ActivityState::Paused);
    }

    // reset exploit-step counter
    ExploitSteps = EXPLOIT_FACTOR;
    return transitionTo(ActivityState::Ready);
  }



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
      DuplicateCompiles++; DuplicateCompilesInARow++;

      if (DuplicateCompilesInARow >= MAX_DUPES_IN_ROW) {
        // give up.
        DuplicateCompilesInARow = 0;
        clogs() << "hit max number of duplicate compiles in a row.\n";
        return transitionTo(ActivityState::Ready);
      }

      clogs() << "compile job produced duplicate code... trying another compile!\n";
      goto retryExperiment;
    }

    std::string NewLib = NewCV.getLibraryName();
    Versions[NewLib] = std::move(NewCV);

    Bakery = Bakeoff(State, this, BP, BestLib, NewLib);

    return transitionTo(ActivityState::TestingNewLib);
  }



  ///////////////////////////////// READY
  assert(Status == ActivityState::Ready);

  // experiment!
  Experiments += 1;
retryExperiment:
  KnobSet NewKnobs = std::move(PBT.getConfig(BestLib));
  NewKnobs.dump();
  Compiler.enqueueCompilation(*Bitcode, std::move(NewKnobs));
  transitionTo(ActivityState::WaitingForCompile);
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
  : TuningSection(TSI, RootFunc),
    PBT(TSI.Config, BaseKnobs, Versions),
    BP(TSI.Config) {
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

  for (auto &Client : State.Clients)
    Client->send_library(Client->State, DylibMsg);
}

void TuningSection::redirectTo(GroupState &State, CodeVersion const& CV) {
  if (CV.isBroken()) {
    warning("trying to redirect to broken lib.");
    return;
  }

  std::string LibName = CV.getLibraryName();
  std::string FuncName = FnGroup.Root;

  // NOTE: this is _partially_ initialized. we let the client modify the addr field
  // before it sends it off (if needed).
  pb::ModifyFunction MF;
  MF.set_name(FuncName);
  MF.set_desired_state(pb::FunctionState::REDIRECTED);
  MF.set_other_lib(LibName);
  MF.set_other_name(FuncName);

  for (auto &Client : State.Clients)
    Client->redirect_to(Client->State, MF);
}


TuningSection::TuningSection(TuningSectionInitializer TSI, std::string RootFunc)
    : FnGroup(RootFunc), Compiler(TSI.Pool, TSI.Pipeline), Profile(TSI.Profile) {
  ////////////
  // Choose the set of all funcs in this tuning section.

  // start off with all functions reachable according to the call-graph
  auto Reachable = Profile.getCallGraph().allReachable(RootFunc);

  // filter down that set to just those for which we have bitcode
  for (auto const& Func : Reachable)
    if (Func.HaveBitcode)
      FnGroup.AllFuncs.insert(Func.Name);

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
  if (!MaybeHotNode){
    info("TuningSection::Create -- no suitable hottest node.");
    return llvm::None;
  }

  auto MaybeAncestor = TSI.Profile.findSuitableTuningRoot(MaybeHotNode.getValue());
  if (!MaybeAncestor) {
    info("TuningSection::Create -- no suitable tuning root.");
    return llvm::None;
  }

  std::string PatchableAncestorName = MaybeAncestor.getValue();

  if (!TSI.Profile.haveBitcode(PatchableAncestorName)) {
    info("TuningSection::Create -- no bitcode available for tuning root.");
    return llvm::None;
  }

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