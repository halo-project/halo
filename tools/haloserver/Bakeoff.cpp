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
  assert(CurIPC.capacity() >= MIN_OBSERVATIONS);
  CurIPC.clear();

  auto &NewIPC = TS->Versions[New].getIPC();
  assert(NewIPC.capacity() >= MIN_OBSERVATIONS);
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
  Other = Deployed;

  DeployedSampledSeen = 0;

  deploy(State, Deployed);
}


bool Bakeoff::hasSufficientObservations(std::string const& LibName) {
  return TS->Versions[LibName].getIPC().size() >= MIN_OBSERVATIONS;
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

  // add the new IPC reading
  auto &DeployedCV = TS->Versions[Deployed];
  DeployedCV.observeIPC(Perf.IPC);

  // do we have enough readings for this one?
  if (!hasSufficientObservations(Deployed))
    return Status;

  // do we have enough readings for the other one?
  if (!hasSufficientObservations(Other)) {
    switchVersions(State);
    return Status;
  }

  // at this point, we're positioned to determine a winner!

  auto &DeployedIPC = TS->Versions.at(Deployed).getIPC();
  auto &OtherIPC = TS->Versions.at(Other).getIPC();

  // TODO: use a more rigorous method to compare means.
  if (OtherIPC.mean() >= DeployedIPC.mean()) {
    Winner = Other;
    switchVersions(State);
  } else {
    Winner = Deployed;
  }

  Status = Result::Finished;
  return Status;
}

} // end namespace