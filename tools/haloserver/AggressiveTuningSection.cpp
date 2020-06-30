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


void AggressiveTuningSection::adjustAfterBakeoff(Bakeoff::Result Result) {
  float Target;

  if (Result == Bakeoff::Result::Timeout || Result == Bakeoff::Result::CurrentIsBetter) {
    // increase the amount of exploit to recover the time.
    // we only ended-up wasting time.
    Target = MAX_TGT_FACTOR;

  } else if (Result == Bakeoff::Result::NewIsBetter) {
    // let's be a bit more aggressive and try to find even better versions.
    // we can make-up the time later!
    Target = MIN_TGT_FACTOR;
    SuccessfulBakeoffs++;

  } else {
    return; // bake-off not done yet
  }

  ExploitFactor += EXPLOIT_LEARNING_RATE * (Target - ExploitFactor);
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
  if (Status == ActivityState::TestingNewLib) {
    assert(Bakery.hasValue() && "no bakery when trying to test a lib?");
    auto &Bakeoff = Bakery.getValue();

    auto Result = Bakeoff.take_step(State);

    // TODO: remove the old way to pay off debt!
    // I think we want to repurpose "paused" state to include the stopping-condition
    // test, in conjunction with the duplicate compiles businesses.
    // adjustAfterBakeoff(Result);

    switch (Result) {
      case Bakeoff::Result::InProgress:
      case Bakeoff::Result::PayingDebt:
        return transitionTo(ActivityState::TestingNewLib);

      case Bakeoff::Result::NewIsBetter:
      case Bakeoff::Result::CurrentIsBetter: {
        auto NewBest = Bakeoff.getWinner();
        if (!NewBest)
          fatal_error("bakeoff successfully finished but no winner?");
        BestLib = NewBest.getValue();

        Bakery = llvm::None;
        return transitionTo(ActivityState::Ready);
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
        return transitionTo(ActivityState::Ready);
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
    ExploitSteps = std::round(ExploitFactor);
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

    std::string NewLib;

    // is it a duplicate?
    if (Dupe) {
      DuplicateCompiles++; DuplicateCompilesInARow++;

      if (DuplicateCompilesInARow < MAX_DUPES_IN_ROW)
        goto retryExperiment;

      DuplicateCompilesInARow = 0; // reset

      if (Versions.size() < 2)
        // we can't explore at all. there's seemingly no code we can generate that's different.
        return transitionTo(ActivityState::Paused);

      clogs() << "Unable to generate a new code version, but we'll retry an existing one.\n";

      NewLib = pickRandomly(PBT.getRNG(), Versions, BestLib);

    } else {
      // not a duplicate!
      NewLib = NewCV.getLibraryName();
      Versions[NewLib] = std::move(NewCV);
    }

    // ok let's evaluate the two libs!
    Bakery = Bakeoff(State, this, BP, BestLib, NewLib);
    Bakeoffs++;
    return transitionTo(ActivityState::TestingNewLib);
  }



  ///////////////////////////////// READY
  assert(Status == ActivityState::Ready);

  // experiment!
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
          << "\n";

  if (Bakery.hasValue())
    Bakery.getValue().dump();


  clogs() << "}\n\n";
}

AggressiveTuningSection::AggressiveTuningSection(TuningSectionInitializer TSI, std::string RootFunc)
  : TuningSection(TSI, RootFunc),
    PBT(TSI.Config, BaseKnobs, Versions),
    BP(TSI.Config),
    MAX_DUPES_IN_ROW(config::getServerSetting<unsigned>("ts-max-dupes-row", TSI.Config)),
    EXPLOIT_LEARNING_RATE(config::getServerSetting<float>("ts-exploit-discount", TSI.Config)),
    MAX_TGT_FACTOR(config::getServerSetting<float>("ts-exploit-max", TSI.Config)),
    MIN_TGT_FACTOR(config::getServerSetting<float>("ts-exploit-min", TSI.Config)),
    ExploitFactor(config::getServerSetting<float>("ts-exploit-init", TSI.Config))
  {
    assert(0 <= EXPLOIT_LEARNING_RATE && EXPLOIT_LEARNING_RATE <= 1.0f);
    assert(MIN_TGT_FACTOR <= ExploitFactor && ExploitFactor <= MAX_TGT_FACTOR);
    // create a version for the original library to record its performance, etc.
    CodeVersion OriginalLib{OriginalLibKnobs};
    std::string Name = OriginalLib.getLibraryName();
    Versions[Name] = std::move(OriginalLib);
    BestLib = Name;
  }
} // namespace halo
