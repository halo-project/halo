#pragma once

#include <cinttypes>

#include "llvm/DebugInfo/Symbolize/Symbolize.h"

namespace sym = llvm::symbolize;

namespace halo {

using IDType = uint64_t;

enum DataKind {
  InstrPtr,
  TimeStamp
};

class Profiler {
private:
  uint64_t FreeID = 0;

  std::string BinaryPath;
  std::string ProcessTriple;
  std::string HostCPUName;

  sym::LLVMSymbolizer *Symbolizer;

public:

  IDType newSample() { return FreeID++; }

  Profiler(std::string BinPath)
             : BinaryPath(BinPath),
               ProcessTriple(llvm::sys::getProcessTriple()),
               HostCPUName(llvm::sys::getHostCPUName()) {

    sym::LLVMSymbolizer::Options Opts;
    Opts.UseSymbolTable = true;
    Opts.Demangle = true;
    Opts.RelativeAddresses = true;
    Opts.DefaultArch = llvm::Triple(ProcessTriple).getArchName().str();

    // there are no copy / assign / move constructors.
    Symbolizer = new sym::LLVMSymbolizer(Opts);
  }

  ~Profiler() {
    delete Symbolizer;
  }

  void recordData1(IDType, DataKind, uint64_t);
  void recordData2(IDType, DataKind, uint64_t, uint64_t);

};


} // end halo namespace
