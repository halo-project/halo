#include "halo/compiler/CodeRegionInfo.h"

namespace halo {

const std::string CodeRegionInfo::UnknownFn = "???";

void CodeRegionInfo::init(pb::ClientEnroll const& CE) {
  AddrMap = CodeMap();
  NameMap.clear();

  NameMap[UnknownFn] = UnknownFI;

  pb::ModuleInfo const& MI = CE.module();
  VMABase = MI.vma_delta();

  // process the address-space mapping
  for (pb::FunctionInfo const& PFI: MI.funcs()) {
    uint64_t Start = PFI.start();
    uint64_t End = Start + PFI.size();
    std::string Name = PFI.label();
    addRegion(Name, Start, End, PFI.patchable());
  }
}

void CodeRegionInfo::addRegion(std::string Name, uint64_t Start, uint64_t End, bool Patchable) {
  auto FuncRange = icl::right_open_interval<uint64_t>(Start, End);

  FunctionInfo *FI = new FunctionInfo(Name, Start, End, Patchable);
  // FI->dump(logs());

  assert(NameMap.find(Name) == NameMap.end() && "trying to overwrite an existing FunctionInfo!");

  NameMap[Name] = FI;
  AddrMap.insert(std::make_pair(FuncRange, FI));
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

} // end namespace halo
