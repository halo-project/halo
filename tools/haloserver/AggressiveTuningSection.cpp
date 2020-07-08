#include "halo/tuner/AggressiveTuningSection.h"
#include "halo/server/ClientGroup.h"
#include "halo/nlohmann/util.hpp"

namespace halo
{

  void AggressiveTuningSection::transitionTo(ActivityState S) {
  ActivityState From = Status;
  ActivityState To = S;

  // self-loop?
  if (From == To)
    return;
  else
    Status = To;  // we actually changed to a different state.
}


std::string pickRandomly(std::mt19937_64 &RNG, std::unordered_map<std::string, CodeVersion> const& Versions, std::string const& ToAvoid) {
  assert(Versions.size() > 1);
  std::uniform_int_distribution<size_t> dist(0, Versions.size()-1);
  std::string NewLib = ToAvoid;

  do {
    size_t Chosen = dist(RNG);
    size_t I = 0;
    for (auto const& Entry : Versions) {

      if (I != Chosen) {
        I++;
        continue;
      }

      NewLib = Entry.first;
      break;
    }

  } while (NewLib == ToAvoid);

  return NewLib;

}


/// The implementation of this take-step always must assume that
/// a client has joined at an arbitrary time. So actions such as
/// enabling/disabling sampling, or sending the current code, should be
/// attempted on every step to make sure they're in the right state.
void AggressiveTuningSection::take_step(GroupState &State) {
  Steps++;

  /////////////////////////// BAKEOFF
  if (Status == ActivityState::Bakeoff) {
    assert(Bakery.hasValue() && "no bakery when trying to test a lib?");
    auto &Bakeoff = Bakery.getValue();

    auto Result = Bakeoff.take_step(State);

    switch (Result) {
      case Bakeoff::Result::InProgress:
      case Bakeoff::Result::PayingDebt:
        return transitionTo(ActivityState::Bakeoff);

      case Bakeoff::Result::NewIsBetter:
        SuccessfulBakeoffs++; // FALL-THROUGH
      case Bakeoff::Result::CurrentIsBetter: {
        auto NewBest = Bakeoff.getWinner();
        if (!NewBest)
          fatal_error("bakeoff successfully finished but no winner?");
        BestLib = NewBest.getValue();

        Bakery = llvm::None;
        return transitionTo(ActivityState::ConsiderStopping);
      };

      case Bakeoff::Result::Timeout: {
        // the two libraries are too similar. we'll merge them.
        BakeoffTimeouts++;
        assert(Bakeoff.getDeployed() != Bakeoff.getOther());

        // we'll keep the currently deployed version
        BestLib = Bakeoff.getDeployed();

        // merge and then remove the other version
        auto Other = Bakeoff.getOther();
        Versions[BestLib].forceMerge(Versions[Other]);
        Versions.erase(Other);

        Bakery = llvm::None;
        return transitionTo(ActivityState::ConsiderStopping);
      };
    };
    fatal_error("unhandled bakeoff case");
  }


retryNonBakeoffStep:
  /////////////
  // Since we're not in a bakeoff, if any clients just
  // (re)joined, get them up-to-speed with the best one!
  //
  assert(Versions.find(BestLib) != Versions.end() && "current version not already in database?");
  sendLib(State, Versions[BestLib]);
  redirectTo(State, Versions[BestLib]);

  /////////////////////////////// STOPPED
  if (Status == ActivityState::Stopped)
    return transitionTo(ActivityState::Stopped);

  // if we're not stopped yet, then we are responsible for controlling the
  // sampling, etc.

  // make sure all clients are not sampling right now
  ClientGroup::broadcastSamplingPeriod(State, 0);


  //////////////////////////// CHECK IF WE SHOULD STOP
  if (Status == ActivityState::ConsiderStopping) {

    if (Stopper.shouldStop(BestLib, Versions, PBT.getConfigManager())) {
      // make sure all clients are not sampling right now
      ClientGroup::broadcastSamplingPeriod(State, 0);
      return transitionTo(ActivityState::Stopped);
    }

    // let's keep going
    return transitionTo(ActivityState::Experiment);
  }



  //////////////////////////// COMPILING
  if (Status == ActivityState::Compiling) {
    // if we ran out of jobs because they were all duplicates,
    // go back to experimenting to queue-up new configurations
    if (Compiler.jobsInFlight() == 0)
      return transitionTo(ActivityState::Experiment);

    // we have a job, but is it done compiling?
    auto CompileDone = Compiler.dequeueCompilation();
    if (!CompileDone) // we'll wait for it
      return transitionTo(ActivityState::Compiling);

    CodeVersion NewCV {std::move(CompileDone.getValue())};

    clogs(LC_Info) << "config for library " << NewCV.getLibraryName() << "\n";
    NewCV.getConfigs().front().dump();

    // check if this is a duplicate
    bool Dupe = false;
    for (auto const& Entries : Versions)
      if ( (Dupe = Versions[Entries.first].tryMerge(NewCV)) )
        break;

    std::string NewLib;

    // is it a duplicate?
    if (Dupe) {
      DuplicateCompiles++; DuplicateCompilesInARow++;

      if (DuplicateCompilesInARow < MAX_DUPES_IN_ROW)
        goto retryNonBakeoffStep;

      DuplicateCompilesInARow = 0; // reset

      return transitionTo(ActivityState::ConsiderStopping);

      // if (Versions.size() < 2)
      //   // we can't explore at all. there's seemingly no code we can generate that's different.
      //   return transitionTo(ActivityState::ConsiderStopping);

      // clogs() << "Unable to generate a new code version, but we'll retry an existing one.\n";
      // NewLib = pickRandomly(PBT.getRNG(), Versions, BestLib);

    } else {
      // not a duplicate!
      NewLib = NewCV.getLibraryName();
      Versions[NewLib] = std::move(NewCV);
    }

    // ok let's evaluate the two libs!
    Bakery = Bakeoff(State, this, BP, BestLib, NewLib);
    Bakeoffs++;
    return transitionTo(ActivityState::Bakeoff);
  }



  ///////////////////////////////// EXPERIMENT
  assert(Status == ActivityState::Experiment);

  // Ask for one config initially, and then keep enqueuing more
  // if it has already pre-determined the next few.
  do {
    Compiler.enqueueCompilation(*Bitcode, std::move(PBT.getConfig(BestLib)));
  } while (PBT.nextIsPredetermined());

  transitionTo(ActivityState::Compiling);
}


void AggressiveTuningSection::dump() const {
  clogs() << "TuningSection for " << FnGroup.Root << " {"\
          << "\n\tAllFuncs = ";

  for (auto const& Func : FnGroup.AllFuncs)
    clogs() << Func << ", ";

  float SuccessRate = Bakeoffs == 0
                        ? 0
                        : 100.0 * ( ((float)SuccessfulBakeoffs) / ((float)Bakeoffs) );

  float TimeoutRate = Bakeoffs == 0
                      ? 0
                      : 100.0 * ( ((float)BakeoffTimeouts) / ((float)Bakeoffs) );


  clogs() << "\n\tStatus = " << stateToString(Status)
          << "\n\tBestLib = " << BestLib
          << "\n\t# Steps = " << Steps
          << "\n\t# Bakeoffs = " << Bakeoffs
          << "\n\tBakeoff Timeout Rate = " << TimeoutRate << "%"
          << "\n\tExperiment Success Rate = " << SuccessRate << "%"
          << "\n\tDuplicateCompiles = " << DuplicateCompiles
          << "\n\tDB Size = " << PBT.getConfigManager().size()
          << "\n";

  if (Bakery.hasValue())
    Bakery.getValue().dump();


  clogs() << "}\n\n";
}

AggressiveTuningSection::AggressiveTuningSection(TuningSectionInitializer TSI, std::string RootFunc)
  : TuningSection(TSI, RootFunc),
    PBT(TSI.Config, BaseKnobs, Versions),
    Stopper(BaseKnobs),
    BP(TSI.Config),
    MAX_DUPES_IN_ROW(config::getServerSetting<unsigned>("ts-max-dupes-row", TSI.Config))
  {
    // create a version for the original library to record its performance, etc.
    CodeVersion OriginalLib{OriginalLibKnobs};
    std::string Name = OriginalLib.getLibraryName();
    Versions[Name] = std::move(OriginalLib);
    BestLib = Name;
  }
} // namespace halo
