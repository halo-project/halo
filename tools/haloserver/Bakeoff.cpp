#include "halo/tuner/Bakeoff.h"
#include "halo/tuner/TuningSection.h"
#include "halo/tuner/CodeVersion.h"
#include "halo/server/ClientGroup.h"
#include "halo/nlohmann/util.hpp"

#include <gsl/gsl_rstat.h>
#include <cmath>
#include <algorithm>

namespace halo {

BakeoffParameters::BakeoffParameters(nlohmann::json const& Config) {
  SWITCH_RATE = config::getServerSetting<size_t>("bakeoff-switch-rate", Config);
  MAX_SWITCHES = config::getServerSetting<size_t>("bakeoff-max-switches", Config);
  MIN_SAMPLES = config::getServerSetting<size_t>("bakeoff-min-samples", Config);

  assert(MIN_SAMPLES >= 2);
  assert(MAX_SWITCHES > 0);
  assert(SWITCH_RATE > 0);

  size_t confidenceInt = config::getServerSetting<size_t>("bakeoff-confidence", Config);
  if (confidenceInt == 95)
    CONFIDENCE = 0.95f;
  else if (confidenceInt == 99)
    CONFIDENCE = 0.99f;
  else
    fatal_error("unknown confidence level " + std::to_string(confidenceInt));
}

Bakeoff::Bakeoff(GroupState &State, TuningSection *TS, BakeoffParameters BP, std::string Current, std::string New)
    : BP(BP), TS(TS),
    NEW_LIBNAME(New), Deployed(Current, {}), Other(New, {}),
    Status(Result::InProgress) {

  assert(Deployed != Other);
  assert(TS->Versions.find(Current) != TS->Versions.end() && "Current not in version database?");
  assert(TS->Versions.find(New) != TS->Versions.end() && "New not in version database?");

  StepsUntilSwitch = BP.SWITCH_RATE;

  // start off by clearing out previous performance data from both versions.
  auto &CurIPC = TS->Versions[Current].getIPC();
  assert(CurIPC.capacity() > 0);
  CurIPC.clear();

  auto &NewIPC = TS->Versions[New].getIPC();
  assert(NewIPC.capacity() > 0);
  NewIPC.clear();

  // make sure the clients are in the state we expect
  deploy(State, Deployed.first);
}

llvm::Optional<std::string> Bakeoff::getWinner() const {
  if (Status == Result::NewIsBetter || Status == Result::CurrentIsBetter)
    return Winner;
  return llvm::None;
}


void Bakeoff::deploy(GroupState &State, std::string const& Name) {
  TS->sendLib(State, TS->Versions[Name]);
  TS->redirectTo(State, TS->Versions[Name]);
}


void Bakeoff::switchVersions(GroupState &State) {
  std::swap(Deployed, Other);

  Switches += 1;
  StepsUntilSwitch = BP.SWITCH_RATE;

  deploy(State, Deployed.first);
}


Bakeoff::Result Bakeoff::transition_to_debt_repayment(GroupState &State) {
  // turns off sampling asap.
  ClientGroup::broadcastSamplingPeriod(State, 0);

  // FIXME:
  // 1. this calculation assumes that 100% of time is spent executing the function in
  //    the future. We could figure out what % of time is actually spent executing here
  //    by looking at ratio of total samples to samples in the lib. This would increase
  //    the number of steps left to repay, I think.
  //    For now, all of my benchmarks focus solely on basically one TS.
  //
  // 2. We could slightly discount the observations in TotalAvg to account for overhead
  //    of switching in the bakeoff. So far that seems unnessecary.
  //

  gsl_rstat_workspace *stats = gsl_rstat_alloc();

  // First, lets find the avg of the winner
  for (auto const& Entry : History)
    if (Entry.first == Winner)
      gsl_rstat_add(Entry.second.IPC, stats);

  // t_best, average IPC of the best library during bakeoff
  double BestAvg = gsl_rstat_mean(stats);

  // next lets add the remaining observed IPCs
  // to determine how far behind we are on work.

  for (auto const& Entry : History)
    if (Entry.first != Winner)
      gsl_rstat_add(Entry.second.IPC, stats);

  // x_bar, average IPC during the entire bakeoff
  double TotalAvg = gsl_rstat_mean(stats);

  // N, the number of IPC observations during the bakeoff
  // double N = gsl_rstat_n(stats);

  double Delta = BestAvg - TotalAvg;
  while (std::abs(Delta) > 0.5) { // TODO: maybe this is a parameter? or a percentage of the best IPC?
    // simulate what IPC might be like after one time-step using best lib
    PaymentsRemaining++;
    gsl_rstat_add(BestAvg, stats);

    // how far behind are we now?
    Delta = BestAvg - gsl_rstat_mean(stats);
  }

  gsl_rstat_free(stats);
  Status = Result::PayingDebt;
  return Status;
}


Bakeoff::Result Bakeoff::debt_payment_step(GroupState &State) {
  assert(Status == Result::PayingDebt);

  // make sure sampling is off
  ClientGroup::broadcastSamplingPeriod(State, 0);

  PaymentsRemaining--;

  if (PaymentsRemaining)
    return Status;

  // done paying debt! announce winner
  if (Winner.hasValue())
    Status = (Winner.getValue() == NEW_LIBNAME
                ? Result::NewIsBetter
                : Result::CurrentIsBetter);
  else
    Status = Result::Timeout;

  return Status;
}


Bakeoff::Result Bakeoff::take_step(GroupState &State) {

  // make sure all clients, including those who just connected, are participating
  deploy(State, Deployed.first);

  if (Status == Result::PayingDebt)
    return debt_payment_step(State);

  if (Status != Result::InProgress)
    return Status;

  assert(Status == Result::InProgress);

  // make sure all clients are sampling right now
  ClientGroup::broadcastSamplingPeriod(State, TS->Profile.getSamplePeriod());

    // update CCT etc
  TS->Profile.consumePerfData(State);

  // first, check for fresh perf info
  TSPerf Perf = TS->Profile.currentPerf(TS->FnGroup, Deployed.first);

  // no new samples in this library? can't make progress
  if (Deployed.second.SamplesSeen == Perf.SamplesSeen) {
    info("Bakeoff can't make progress b/c no samples observed in the deployed library.");
    return Status;
  } else {
    Deployed.second = Perf;
  }

  History.emplace_back(Deployed.first, Perf);

  auto &DeployedIPC = TS->Versions.at(Deployed.first).getIPC();
  auto &OtherIPC = TS->Versions.at(Other.first).getIPC();

  // add the new IPC reading
  DeployedIPC.observe(Perf.IPC);

  // try to determine a winner
  switch(compare_ttest(DeployedIPC, OtherIPC)) {
    case ComparisonResult::GreaterThan: {
      Winner = Deployed.first;
      return transition_to_debt_repayment(State);
    }; break;

    case ComparisonResult::LessThan: {
      Winner = Other.first;
      switchVersions(State);  // switch to better version.
      return transition_to_debt_repayment(State);
    }; break;

    case ComparisonResult::NoAnswer: {
      // set-up for the next iteration of the bake-off

      if (Switches >= BP.MAX_SWITCHES) {
        // we give up... don't know which one is better!

        // prefer going back to the original one when timing out.
        if (Deployed.first == NEW_LIBNAME)
          switchVersions(State);

        return transition_to_debt_repayment(State);
      }

      StepsUntilSwitch -= 1;
      if (StepsUntilSwitch == 0)
        switchVersions(State);

    };
  };

  return Status;
}

Bakeoff::ComparisonResult Bakeoff::compare_means(RandomQuantity const& A, RandomQuantity const& B) const {
  if (A.size() < BP.MIN_SAMPLES || B.size() < BP.MIN_SAMPLES)
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

// yes its ugly.
//
// This is Table II from "The Statistical Analysis of Experimental Data" by John Mandel. First edition (1964)
//
// confidence-level -> (degrees-of-freedom, threshold) list
static const std::map<float, std::vector<std::pair<unsigned, double>>> ThresholdTable = {
  {0.95f, {
      {1, 6.314},
      {2, 2.920},
      {3, 2.353},
      {4, 2.132},
      {5, 2.015},
      {6, 1.943},
      {7, 1.895},
      {8, 1.860},
      {9, 1.833},
      {10, 1.812},
      {11, 1.796},
      {12, 1.782},
      {13, 1.771},
      {14, 1.761},
      {15, 1.753},
      {16, 1.746},
      {17, 1.740},
      {18, 1.734},
      {19, 1.729},
      {20, 1.725},
      {21, 1.721},
      {22, 1.717},
      {23, 1.714},
      {24, 1.711},
      {25, 1.708},
      {26, 1.706},
      {27, 1.703},
      {28, 1.701},
      {29, 1.699},
      {30, 1.697},
      {40, 1.684},
      {60, 1.671},
      {120, 1.658},
      {std::numeric_limits<unsigned>::max(), 1.645},
    }},
    {0.99f, {
      {1, 31.821},
      {2, 6.965},
      {3, 4.541},
      {4, 3.747},
      {5, 3.365},
      {6, 3.143},
      {7, 2.998},
      {8, 2.896},
      {9, 2.821},
      {10, 2.764},
      {11, 2.718},
      {12, 2.681},
      {13, 2.650},
      {14, 2.624},
      {15, 2.602},
      {16, 2.583},
      {17, 2.567},
      {18, 2.552},
      {19, 2.539},
      {20, 2.528},
      {21, 2.518},
      {22, 2.508},
      {23, 2.500},
      {24, 2.492},
      {25, 2.485},
      {26, 2.479},
      {27, 2.473},
      {28, 2.467},
      {29, 2.462},
      {30, 2.457},
      {40, 2.423},
      {60, 2.390},
      {120, 2.358},
      {std::numeric_limits<unsigned>::max(), 2.326},
    }}
};

// you should make sure to use the exact float literal, with the f suffix, when calling this function!!
double t_threshold(const float confidenceLevel, unsigned degrees_of_freedom) {
  assert(degrees_of_freedom != 0 && "invalid dof");

  auto Result = ThresholdTable.find(confidenceLevel);

  if (Result == ThresholdTable.end())
    fatal_error("unable to find t threshold column for confidence level " + std::to_string(confidenceLevel));

  auto const& Column = Result->second;
  assert(Column.size() >= 2); // we start at the 2nd element.

  // covers case of dof == 1
  if (degrees_of_freedom == Column[0].first)
    return Column[0].second;

  // we generally round up. for example, if dof is between 40 and 80, we will always pick the latter's threshold.
  for (size_t i = 1; i < Column.size(); i++) {
    // check for equality or right-boundedness
    if (degrees_of_freedom <= Column[i].first)
      return Column[i].second;
  }

  fatal_error("no threshold found  for df = " + std::to_string(degrees_of_freedom));
}

Bakeoff::ComparisonResult Bakeoff::compare_ttest(RandomQuantity const& A, RandomQuantity const& B) const {
  if (A.size() < BP.MIN_SAMPLES || B.size() < BP.MIN_SAMPLES)
    return ComparisonResult::NoAnswer;

  /*
    We perform a two-sample t test aka Welch's unequal variances t-test.
    More details:
      1. https://en.wikipedia.org/wiki/Welch%27s_t-test
      2. Section 10.2 of Devore & Berk's
          "Modern Mathematical Statistics with Applications"

    let DELTA = 0

    Null Hypothesis:
      a_mean - b_mean = DELTA   -- the means are the same

    Alternative Hypothesis 1:
      a_mean - b_mean > DELTA  -- A > B

      Reject null hypothesis in favor of this with confidence level 100(1-alpha)% if:
        test_statistic >= t_{alpha, degrees of freedom}

    Alternative Hypothesis 2:
      a_mean - b_mean < DELTA  -- A < B

      Reject null hypothesis in favor of this with confidence level 100(1-alpha)% if:
        test_statistic <= -t_{alpha, degrees of freedom}

    Alternative Hypothesis 3:
      a_mean - b_mean =/= 0

        Reject null hypothesis in favor of this with confidence level 100(1-alpha)% if:
          rejection criteria for any prior hypotheses are met.
  */

  double a_mean = A.mean();
  double a_var = A.variance(a_mean);
  double a_sz = A.size();
  double a_scaledVar = a_var / a_sz;

  double b_mean = B.mean();
  double b_var = B.variance(b_mean);
  double b_sz = B.size();
  double b_scaledVar = b_var / b_sz;

  const double DELTA = 0; // the DELTA in the standard formulation of null hypothesis.

  double test_statistic = (a_mean - b_mean - DELTA) / std::sqrt(a_scaledVar + b_scaledVar);

  // degrees of freedom
  double df = std::trunc( // round down
                std::pow(a_scaledVar + b_scaledVar, 2) /
                    ( (std::pow(a_scaledVar, 2) / (a_sz - 1))
                    + (std::pow(b_scaledVar, 2) / (b_sz - 1))
                  ));


  const double THRESH = t_threshold(BP.CONFIDENCE, df);

  // clogs() << "t = " << test_statistic
  //         << ", df = " << df
  //         << ", thresh = " << THRESH
  //         << "\n";

  const bool Hypo1 = test_statistic >= THRESH;
  const bool Hypo2 = test_statistic <= -THRESH;

  if (Hypo1 && Hypo2) // neither greater or less, but we can't tell which with sufficient certianty.
    return ComparisonResult::NoAnswer;

  if (Hypo1)  // reject null-hypothesis; evidence says A > B
    return ComparisonResult::GreaterThan;

  if (Hypo2)  // reject null-hypothesis; evidence says A < B
    return ComparisonResult::LessThan;

  return ComparisonResult::NoAnswer;  // neither Hypo1 nor Hypo2 is true.
}


void Bakeoff::dump() const {
  clogs() << "\tBakeoff: {"
          << "\n\t\tSteps = " << Switches
          << "\n\t\tStepsUntilSwitch = " << StepsUntilSwitch
          << "\n\t\tCurrently Deployed = " << Deployed.first
          << "\n\t\tPaymentsRemaining = " << PaymentsRemaining
          << "\n\t\tHistory = [\n";

  for (auto const& Entry : History) {
    clogs() << "\t\t\t" << Entry.first << " : ";
    Entry.second.dump();
  }


  clogs() << "\t\t]\n\t}";
}



} // end namespace