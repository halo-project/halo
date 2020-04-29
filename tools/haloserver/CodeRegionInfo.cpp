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
    auto Start = PFI.start();
    auto End = Start + PFI.size();
    FunctionDefinition Def(PFI.label(), PFI.patchable(), Start, End);
    addRegion(Def);
  }
}

void CodeRegionInfo::addRegion(FunctionDefinition const& Def) {

  auto FuncRange = icl::right_open_interval<uint64_t>(Def.Start, Def.End);

  std::shared_ptr<FunctionInfo> FI = nullptr;

  // first, check if this definition's code in memory overlaps with an existing function.
  auto AddrResult = AddrMap.find(FuncRange);

  if (AddrResult != AddrMap.end()) {
    // it overlaps!
    auto Lower = AddrResult->first.lower();
    auto Upper = AddrResult->first.upper();

    assert(Lower == Def.Start && Upper == Def.End && "function overlap is not exact?");

    // then this definition is a name alias with an existing FunctionInfo at the
    // same place in memory!
    FunctionInfo *BareFI = AddrResult->second;
    BareFI->addDefinition(Def);

    // clogs() << "NOTE: function region "
    //         << Def.Name               << " @ [" << Def.Start << ", " << Def.End << ") "
    //         << "\noverlaps with existing region\n "
    //         << FI->getCanonicalName() << " @ [" << Lower     << ", " << Upper << ")"
    //         << ". adding as an alias.\n";

    return;
  }

  // Otherwise, it's possible to have multiple definitions of a function with
  // the same name but they are disjoint in memory. Thus, there are multiple implementations.
  auto NameResult = NameMap.find(Def.Name);

  if (NameResult != NameMap.end()) {
    // then it's a disjoint implementation of a function with the same name.
    FI = NameResult->second;
    FI->addDefinition(Def);
  } else {
    // have not seen this function definition at all, so make a fresh FunctionInfo
    FI = std::make_shared<FunctionInfo>(Def);
    NameMap[Def.Name] = FI;
  }

  assert(FI != nullptr);
  // finally, add to the addr map the range it spans.
  AddrMap.insert(std::make_pair(FuncRange, FI.get()));
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
  // NOT function-info pointer inequality since JIT'd may have different
  // pointer but represent the same function!

  if (!(Source->matchingName(Target)))
    return true;

  // okay now we know it's a branch within the same function, so we
  // check to see if the target IP is at the start of its FI.
  // this would indicate a self-recursive call.

  if (Target->hasStart(TgtIP))
    return true;

  return false;
}

std::shared_ptr<FunctionInfo> CodeRegionInfo::lookup(uint64_t IP) const {
  IP -= VMABase;

  auto Result = AddrMap.find(IP);
  if (Result == AddrMap.end())
    return lookup(UnknownFn);

  FunctionInfo *BareFI = Result->second;

  // we need to return the shared_ptr, and since the interval map can't handle
  // shared ptrs, we do a lookup via the name corresponding to the particular
  // definition of the returned function corresponding
  auto MaybeDef = BareFI->getDefinition(IP, false);
  if (!MaybeDef.hasValue())
      fatal_error("inconsistency between addr map and definitions in function info.");

  return lookup(MaybeDef.getValue().Name);
}

std::shared_ptr<FunctionInfo> CodeRegionInfo::lookup(std::string const& Name) const {
  auto Result = NameMap.find(Name);
  if (Result == NameMap.end())
    return lookup(UnknownFn);

  return Result->second;
}

} // end namespace halo
