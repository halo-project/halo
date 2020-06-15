#include "halo/tuner/Bakeoff.h"
#include "halo/tuner/TuningSection.h"
#include "halo/tuner/CodeVersion.h"

#include <gsl/gsl_vector.h>
#include <gsl/gsl_statistics_double.h>
#include <cmath>

namespace halo {


Bakeoff::Bakeoff(GroupState &State, TuningSection *TS, std::string Current, std::string New)
    : TS(TS), Deployed(Current), Other(New), Status(Result::InProgress), DeployedSampledSeen(0) {

  assert(TS->Versions.find(Current) != TS->Versions.end() && "Current not in version database?");
  assert(TS->Versions.find(New) != TS->Versions.end() && "New not in version database?");

  // start off by clearing out previous performance data from both versions.
  auto &CurIPC = TS->Versions[Current].getIPC();
  assert(CurIPC.capacity() > 0);
  CurIPC.clear();

  auto &NewIPC = TS->Versions[New].getIPC();
  assert(NewIPC.capacity() > 0);
  NewIPC.clear();

  // make sure the clients are in the state we expect
  deploy(State, Deployed);
}

llvm::Optional<std::string> Bakeoff::getWinner() const {
  return Winner;
}


void Bakeoff::deploy(GroupState &State, std::string const& Name) {
  TS->sendLib(State, TS->Versions[Name]);
  TS->redirectTo(State, TS->Versions[Name]);
}


void Bakeoff::switchVersions(GroupState &State) {
  std::string Temp = Deployed;
  Deployed = Other;
  Other = Temp;

  DeployedSampledSeen = 0;

  deploy(State, Deployed);
}


Bakeoff::Result Bakeoff::take_step(GroupState &State) {
  if (Status != Result::InProgress)
    return Status;

  // make sure all clients, including those who just connected, are participating
  deploy(State, Deployed);

  // first, check for fresh perf info
  GroupPerf Perf = TS->Profile.currentPerf(TS->FnGroup, Deployed);

  // no new samples in this library? can't make progress
  if (DeployedSampledSeen == Perf.SamplesSeen)
    return Status;
  else
    DeployedSampledSeen = Perf.SamplesSeen;


  auto &DeployedIPC = TS->Versions.at(Deployed).getIPC();
  auto &OtherIPC = TS->Versions.at(Other).getIPC();

  // add the new IPC reading
  DeployedIPC.observe(Perf.IPC);


  // try to determine a winner
  switch(compare_means(DeployedIPC, OtherIPC)) {
    case ComparisonResult::GreaterThan: {
      Winner = Deployed;
      Status = Result::Finished;
    }; break;

    case ComparisonResult::LessThan: {
      Winner = Other;
      switchVersions(State);  // switch to better version.
      Status = Result::Finished;
    }; break;

    case ComparisonResult::NoAnswer: {
      // we'll stay in this state and try again next time step.
      StepsUntilSwitch -= 1;
      if (StepsUntilSwitch == 0) {
        StepsUntilSwitch = SWITCH_RATE;
        switchVersions(State);
      }
    };
  };

  return Status;
}

Bakeoff::ComparisonResult Bakeoff::compare_means(RandomQuantity const& A, RandomQuantity const& B) const {
  constexpr size_t MIN_OBSERVATIONS = 5;

  if (A.size() < MIN_OBSERVATIONS || B.size() < MIN_OBSERVATIONS)
    return ComparisonResult::NoAnswer;

  auto meanA = A.mean();
  auto meanB = B.mean();

  if (meanA < meanB)
    return ComparisonResult::LessThan;
  else if (meanA > meanB)
    return ComparisonResult::GreaterThan;
  else
    return ComparisonResult::NoAnswer;
}

Bakeoff::ComparisonResult Bakeoff::compare_ttest(RandomQuantity const& A, RandomQuantity const& B) const {
  return ComparisonResult::NoAnswer;
}

} // end namespace