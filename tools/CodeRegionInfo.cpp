#include "halo/CodeRegionInfo.h"

namespace halo {

void CodeRegionInfo::init(pb::ClientEnroll const& CE) {
  pb::ModuleInfo const& MI = CE.module();

  VMABase = MI.vma_delta();

  for (pb::FunctionInfo const& FI: MI.funcs()) {
    uint64_t Start = FI.start();
    uint64_t End = Start + FI.size();
    auto FuncRange = icl::right_open_interval<uint64_t>(Start, End);

    auto Info = std::shared_ptr<FunctionInfo>(new FunctionInfo(FI.label()));

    AddrMap.insert(std::make_pair(FuncRange, std::move(Info)));
  }
}

llvm::Optional<FunctionInfo*> CodeRegionInfo::lookup(uint64_t IP) const {
  IP -= VMABase;

  auto FI = AddrMap.find(IP);
  if (FI == AddrMap.end())
    return llvm::None;

  return FI->second.get();
}

// some old code dealing with parsing of LLVM bitcode

// There's a potential for a double free if the unique_ptr to LLVMModule outlives the LLVMContext.

// lvm::SMDiagnostic Err;
// auto MemBuf = llvm::MemoryBuffer::getMemBuffer(MaybeData.get());
// auto Module = llvm::parseIR(*MemBuf, Err, *Info.Cxt);
//
// if (Module.get() == nullptr) {
//   Err.print(ObjPath.c_str(), llvm::errs());
//   halo::fatal_error("invalid bitcode.");
// }
// Info.Module = std::move(Module);

} // end namespace
