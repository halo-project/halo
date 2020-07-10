#include "halo/compiler/ExecutionTimeProfiler.h"

namespace halo {

llvm::Optional<double> ExecutionTimeProfiler::getAvg(std::string const& FuncName) {
  auto Result = AvgTime.find(FuncName);
  if (Result == AvgTime.end())
    return llvm::None;
  return Result->second;
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

    // tweak the time unit
    const double NANO_PER_MILLI = 1000000.0;
    const int MILLIS = 10;

    const double TimeUnits = ElapsedTime / (MILLIS * NANO_PER_MILLI);

    const double CallsPerTimeUnit = ElapsedCalls /  TimeUnits;

    // not seen before? start at this observation.
    if (AvgTime.find(FuncName) == AvgTime.end()) {
      AvgTime[FuncName] = CallsPerTimeUnit;

    } else {
      // update with discount.
      const double DISCOUNT = 0.7;
      AvgTime[FuncName] += DISCOUNT * (CallsPerTimeUnit - AvgTime[FuncName]);

    }

    clogs(LC_Warning) << FuncName << ": elapsed calls = " << ElapsedCalls
                      << ", calls per " << MILLIS << "ms = " << CallsPerTimeUnit
                      << ", avg = " << AvgTime[FuncName] << "\n";
  }

}

} // namespace halo