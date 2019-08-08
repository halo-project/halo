#include "halo/CodeRegionInfo.h"

namespace halo {

void CodeRegionInfo::init(pb::ClientEnroll const& CE) {
  AddrMap = CodeMap();
  NameMap.clear();

  NameMap[UnknownFn] = UnknownFI;

  pb::ModuleInfo const& MI = CE.module();
  VMABase = MI.vma_delta();

  for (pb::FunctionInfo const& PFI: MI.funcs()) {
    uint64_t Start = PFI.start();
    uint64_t End = Start + PFI.size();
    std::string Name = PFI.label();
    auto FuncRange = icl::right_open_interval<uint64_t>(Start, End);

    FunctionInfo *FI = new FunctionInfo(Name);

    NameMap[Name] = FI;
    AddrMap.insert(std::make_pair(FuncRange, FI));
  }
}

FunctionInfo* CodeRegionInfo::lookup(uint64_t IP) const {
  IP -= VMABase;

  auto FI = AddrMap.find(IP);
  if (FI == AddrMap.end())
    return lookup(UnknownFn);

  return FI->second;
}

FunctionInfo* CodeRegionInfo::lookup(std::string const& Name) const {
  auto FI = NameMap.find(Name);
  if (FI == NameMap.end())
    return lookup(UnknownFn);

  return FI->second;
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
