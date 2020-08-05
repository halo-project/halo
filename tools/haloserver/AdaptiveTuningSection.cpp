#include "halo/tuner/AdaptiveTuningSection.h"
#include "halo/server/ClientGroup.h"
#include "halo/nlohmann/util.hpp"

#include "llvm/Support/CommandLine.h"

namespace cl = llvm::cl;

static cl::opt<bool> CL_ForceMerge(
  "halo-forcemerge",
  cl::desc("Forcibly merge the libraries if a bakeoff timed out."),
  cl::init(false));

namespace halo
{

void AdaptiveTuningSection::transitionTo(ActivityState S) {
  ActivityState From = Status;
  ActivityState To = S;

  // self-loop?
  if (From == To)
    return;

  assert(S != ActivityState::Bakeoff && "bakeoff has a dedicated fresh-transition function!");
  assert(S != ActivityState::Waiting && "waiting has dedicated fresh-transition function!");
  assert(S != ActivityState::MakeDecision && "make-decision has dedicated fresh-transition function!");

  Status = To;  // we actually changed to a different state.
}

void AdaptiveTuningSection::transitionToWait() {
  assert(Status != ActivityState::Waiting);

  WaitStepsRemaining = StepsPerWaitAction;
  Status = ActivityState::Waiting;
}

void AdaptiveTuningSection::transitionToBakeoff(GroupState &State, std::string const& NewLib) {
  assert(Status != ActivityState::Bakeoff);

  // ok let's evaluate the two libs!
  Bakery = Bakeoff(State, this, BP, BestLib, NewLib);
  Bakeoffs++;
  Status = ActivityState::Bakeoff;
}

void AdaptiveTuningSection::transitionToDecision(float Reward) {
  assert(Status != ActivityState::MakeDecision);

  MAB.reward(CurrentAction, Reward);
  Status = ActivityState::MakeDecision;
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

float AdaptiveTuningSection::computeReward() {
  // NOTE: relying on assumption that larger "quality"  means better
  auto const& CurrentQ = Versions.at(BestLib).getQuality();
  float Reward = CurrentQ.mean();

  // big reward for this action if the bake-off succeeded
  if (Bakery.hasValue()) {
    auto &Bakeoff = Bakery.getValue();
    if (Bakeoff.lastResult() == Bakeoff::Result::NewIsBetter)
      Reward *= 10;

    Bakery = llvm::None;
  }

  return Reward;
}



/// The implementation of this take-step always must assume that
/// a client has joined at an arbitrary time. So actions such as
/// enabling/disabling sampling, or sending the current code, should be
/// attempted on every step to make sure they're in the right state.
void AdaptiveTuningSection::take_step(GroupState &State) {
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

        return transitionToDecision(computeReward());
      };

      case Bakeoff::Result::Timeout: {
        // the two libraries are too similar.
        BakeoffTimeouts++;
        assert(Bakeoff.getDeployed() != Bakeoff.getOther());

        // we'll keep the currently deployed version
        BestLib = Bakeoff.getDeployed();

        if (CL_ForceMerge) {
          // merge and then remove the other version
          auto Other = Bakeoff.getOther();
          Versions[BestLib].forceMerge(Versions[Other]);
          Versions.erase(Other);
        }

        return transitionToDecision(computeReward());
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

  // make sure all clients are not sampling right now
  ClientGroup::broadcastSamplingPeriod(State, 0);

  /////////////////////////////// WAITING
  // the stopped state means we've given up on trying to compile a
  // new version of code, and instead will now just periodically
  // retry some of the better ones.
  if (Status == ActivityState::Waiting) {
    if (WaitStepsRemaining) {
      WaitStepsRemaining--;
      return transitionTo(ActivityState::Waiting);
    }
    return transitionToDecision(computeReward());
  }


  //////////////////////////// WHAT SHOULD WE DO?
  if (Status == ActivityState::MakeDecision) {
    // consult MAB
    CurrentAction = MAB.choose();

    if (CurrentAction == RunExperiment)
      return transitionTo(ActivityState::Experiment);

    if (CurrentAction == Wait)
      return transitionToWait();

    fatal_error("unhandled decision from MAB!");
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
    TotalCompiles++;

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

      if (Versions.size() < 2)
        // we can't explore at all. there's seemingly no code we can generate that's different.
        return transitionToWait();

      clogs(LC_Info) << "Unable to generate a new code version, but we'll retry an existing one.\n";
      NewLib = pickRandomly(PBT.getRNG(), Versions, BestLib);

    } else {
      // not a duplicate!
      NewLib = NewCV.getLibraryName();
      Versions[NewLib] = std::move(NewCV);
    }

    // ok let's evaluate the two libs!
    return transitionToBakeoff(State, NewLib);
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


void AdaptiveTuningSection::dump() const {
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

  float UniqueCompileRate = TotalCompiles == 0 ? 0
                            : 100.0 * (1.0 - (DuplicateCompiles / static_cast<double>(TotalCompiles)));

  clogs() << "\n\tStatus = " << stateToString(Status)
          << "\n\tBestLib = " << BestLib
          << "\n\t# Steps = " << Steps
          << "\n\t# Bakeoffs = " << Bakeoffs
          << "\n\tBakeoff Timeout Rate = " << TimeoutRate << "%"
          << "\n\tExperiment Success Rate = " << SuccessRate << "%"
          << "\n\tUniqueCompileRate = " << UniqueCompileRate << "%"
          << "\n\tDB Size = " << PBT.getConfigManager().size()
          << "\n";

  if (Bakery.hasValue())
    Bakery.getValue().dump();


  clogs() << "}\n\n";
}

AdaptiveTuningSection::AdaptiveTuningSection(TuningSectionInitializer TSI, std::string RootFunc)
  : TuningSection(TSI, RootFunc),
    PBT(TSI.Config, BaseKnobs, Versions),
    MAB(RootActions),
    BakeoffPenalty(1.0f - config::getServerSetting<float>("bakeoff-assumed-overhead", TSI.Config)),
    StepsPerWaitAction(config::getServerSetting<unsigned>("ts-steps-per-wait", TSI.Config)),
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
