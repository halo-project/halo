#pragma once

#include <cinttypes>


namespace halo {

using IDType = uint64_t;

enum DataKind {
  InstrPtr,
  TimeStamp
};

class Profiler {
private:
  uint64_t FreeID = 0;


public:

  IDType newSample() { return FreeID++; }

  Profiler() {

  }

  void recordData1(IDType, DataKind, uint64_t);
  void recordData2(IDType, DataKind, uint64_t, uint64_t);
};


} // end halo namespace
