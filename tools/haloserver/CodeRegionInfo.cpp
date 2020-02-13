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

bool CodeRegionInfo::isCall(uint64_t SrcIP, uint64_t TgtIP) const {
  auto Source = lookup(SrcIP);
  auto Target = lookup(TgtIP);

  bool SourceUnknown = (Source == UnknownFI);
  bool TargetUnknown = (Target == UnknownFI);

  if (SourceUnknown && TargetUnknown)
    return false;
  else if (SourceUnknown || TargetUnknown)
    return true;

  // NOTE: in the case of both not unknown, we first check for function name inequality,
  // NOT function-info pointer inequality since JIT'd code will have different
  // pointer but represent the same function!

  if (Source->getName() != Target->getName())
    return true;

  // okay now we know it's a branch within the same function, so we
  // check to see if the target IP is at the start of its FI.
  // this would indicate a self-recursive call.

  if (Target->getStart() == TgtIP)
    return true;

  return false;
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
