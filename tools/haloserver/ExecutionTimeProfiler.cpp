#include "halo/compiler/ExecutionTimeProfiler.h"
#include "halo/nlohmann/util.hpp"

namespace halo {

ExecutionTimeProfiler::ExecutionTimeProfiler(JSON const& Config)
  : DISCOUNT(config::getServerSetting<double>("callfreq-discount", Config)) {}

CallFreq ExecutionTimeProfiler::get(std::string const& FuncName) {
  // construct from default init if not found.
  return Data[FuncName];
}

void ExecutionTimeProfiler::observe(ClientID ID, CodeRegionInfo const& CRI, std::vector<pb::CallCountData> const& AllData) {
  for (auto const& Item : AllData)
    observeOne(ID, CRI, Item);
}

void ExecutionTimeProfiler::observeOne(ClientID ID, CodeRegionInfo const& CRI, pb::CallCountData const& CCD) {
  // get last / this time, and update
  const uint64_t ThisTime = CCD.timestamp();
  auto& State = Last[ID];

  for (auto const& Entry : CCD.function_counts()) {
    auto const& FuncName = CRI.lookup(Entry.first)->getCanonicalName();

    uint64_t ThisCallCount = Entry.second;
    uint64_t LastCallCount = State[FuncName].CallCount;
    State[FuncName].CallCount = ThisCallCount;

    assert(ThisCallCount >= LastCallCount && "decreasing call count?");

    if (ThisCallCount == LastCallCount)
      continue; // no calls have occurred since the last time. keep old timestamp.

    // get and update timestamp
    auto LastTime = State[FuncName].Timestamp;
    State[FuncName].Timestamp = ThisTime;

    if (LastTime == 0)
      continue; // need 2 timestamps to compute an elapsed time

    assert(ThisTime >= LastTime && "time went backwards?");

    // the nanoseconds that elapsed since this function was last called
    const double ElapsedTime = ThisTime - LastTime;
    const double ElapsedCalls = ThisCallCount - LastCallCount;
    assert(ElapsedCalls > 0);
    assert(ElapsedTime > 0);

    auto &Avg = Data[FuncName];

    const double NANO_PER_MILLI = 1000000.0;

    auto ComputeCallsPerTimeUnit = [&](unsigned msPerCall) -> double {
      assert(msPerCall != 0);
      const double TimeUnits = ElapsedTime / (msPerCall * NANO_PER_MILLI);
      const double CallsPerTimeUnit = ElapsedCalls /  TimeUnits;
      return CallsPerTimeUnit;
    };

    if (Avg.MilliPerCall != 0) {
      // update with discount.
      Avg.Value += DISCOUNT * (ComputeCallsPerTimeUnit(Avg.MilliPerCall) - Avg.Value);

    } else { // initialize based on this observation.

      // we compute this function's MilliPerCall and initialize the average
      Avg.MilliPerCall = std::ceil((ElapsedTime / ElapsedCalls) / NANO_PER_MILLI);
      assert(Avg.MilliPerCall != 0);

      Avg.Value = ComputeCallsPerTimeUnit(Avg.MilliPerCall);
    }

    Avg.SamplesSeen += 1;


    clogs(LC_Info) << FuncName << ": elapsed calls = " << ElapsedCalls
                      << ", calls per " << Avg.MilliPerCall
                      << "ms = " << (Avg.MilliPerCall == 0 ? 0 : ComputeCallsPerTimeUnit(Avg.MilliPerCall))
                      << ", avg = " << Avg.Value << "\n";
  }

}

} // namespace halo