#include "halo/compiler/CodeRegionInfo.h"

namespace halo {


FunctionInfo::FunctionInfo(uint64_t vmaBase, FunctionDefinition const& Def) : VMABase(vmaBase) {
  addDefinition(Def);
}

std::string const& FunctionInfo::getCanonicalName() const {
  assert(!FD.empty() && "did not expect an empty definition list!");
  return FD[0].Name;
}

bool FunctionInfo::knownAs(std::string const& other) const {
  for (auto const& D : FD)
    if (D.Name == other)
      return true;

  return false;
}

bool FunctionInfo::matchingName(std::shared_ptr<FunctionInfo> const& Other) const {
  // Ugh, O(n^2) b/c matchesName is O(n)
  for (auto const& D : FD)
    if (Other->knownAs(D.Name))
      return true;

  return false;
}

bool FunctionInfo::isPatchable() const {
  assert(!FD.empty() && "did not expect an empty definition list!");

  for (auto const& D : FD)
    if (D.Patchable == false)
      return false;

  return true;
}

bool FunctionInfo::hasStart(uint64_t IP, bool NormalizeIP) const {
  if (NormalizeIP)
    IP -= VMABase;

  for (auto const& D : FD)
    if (D.Start == IP)
      return true;

  return false;
}

llvm::Optional<FunctionDefinition> FunctionInfo::getDefinition(uint64_t IP, bool NormalizeIP) const {
  if (NormalizeIP)
    IP -= VMABase;

  for (auto const& D : FD)
    if (D.Start <= IP && IP < D.End)
      return D;

  return llvm::None;
}

llvm::Optional<FunctionDefinition> FunctionInfo::getDefinition(std::string const& Lib) const {
  for (auto const& D : FD) {
    if (D.Library == Lib)
      return D;
  }
  return llvm::None;
}

std::vector<FunctionDefinition> const& FunctionInfo::getDefinitions() const {
  return FD;
}

void FunctionInfo::addDefinition(FunctionDefinition const& D) {
  FD.push_back(D);
}

bool FunctionInfo::isUnknown() const {
  for (auto const& D : FD)
    if (D.isKnown())
      return false;

  return true;
}

bool FunctionInfo::isKnown() const { return !(isUnknown()); }

void FunctionInfo::dump(llvm::raw_ostream &out) const {
  out << "FunctionInfo = [";

  for (auto const& D : FD) {
    D.dump(out);
    out << ", ";
  }

  out << "]\n";
}



const std::string CodeRegionInfo::UnknownFn = "???";
const std::string CodeRegionInfo::OriginalLib = "<original>";

void CodeRegionInfo::init(pb::ClientEnroll const& CE) {
  AddrMap = CodeMap();
  NameMap.clear();

  NameMap[UnknownFn] = UnknownFI;

  pb::ModuleInfo const& MI = CE.module();
  VMABase = MI.vma_delta();

  // process the address-space mapping
  for (pb::FunctionInfo const& PFI: MI.funcs())
    addRegion(PFI, OriginalLib, false); // client enrollment gives addresses relative to VMABase
}


void CodeRegionInfo::addRegion(pb::DyLibInfo const& DLI, bool Absolute) {
  std::string const& DylibName = DLI.name();
  for (auto const& Entry : DLI.funcs()) {
    auto const& PFI = Entry.second;
    addRegion(PFI, DylibName, Absolute);
  }
}


void CodeRegionInfo::addRegion(pb::FunctionInfo const& PFI, std::string LibName, bool Absolute) {
  auto Start = PFI.start();
  auto End = Start + PFI.size();

  if (Absolute) {
    // translate to be relative to VMABase, which is what we use internally
    Start -= VMABase;
    End -= VMABase;
  }

  FunctionDefinition Def(LibName, PFI.label(), PFI.patchable(), Start, End);
  addRegion(Def);
}


void CodeRegionInfo::addRegion(FunctionDefinition const& Def, bool Absolute) {
  assert(!Absolute && "this overload assumes the function's start/end is relative to VMABase");

  uint64_t Start = Def.Start;
  uint64_t End = Def.End;

  auto FuncRange = icl::right_open_interval<uint64_t>(Start, End);

  std::shared_ptr<FunctionInfo> FI = nullptr;

  // first, check if this definition's code in memory overlaps with an existing function.
  auto AddrResult = AddrMap.find(FuncRange);

  if (AddrResult != AddrMap.end()) {
    // it overlaps!

  #ifndef NDEBUG
    auto Lower = AddrResult->first.lower();
    auto Upper = AddrResult->first.upper();
  #endif

    // clogs() << Def.Name << " @ [" << Start << ", " << End << ")" << "\n intersects with \n"
    //         << "existing func @ [" << Lower << ", " << Upper << ")\n----\n";

    assert(Lower == Start && Upper == End && "function overlap is not exact?");

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
  } // end overlap test

  // Otherwise, it's possible to have multiple definitions of a function with
  // the same name but they are disjoint in memory. Thus, there are multiple implementations.
  auto NameResult = NameMap.find(Def.Name);

  if (NameResult != NameMap.end()) {
    // then it's a disjoint implementation of a function with the same name.
    FI = NameResult->second;
    FI->addDefinition(Def);
  } else {
    // have not seen this function definition at all, so make a fresh FunctionInfo
    FI = std::make_shared<FunctionInfo>(VMABase, Def);
    NameMap[Def.Name] = FI;
  }

  assert(FI != nullptr);
  // finally, add to the addr map the range it spans.
  AddrMap.insert(std::make_pair(FuncRange, FI.get()));
}



bool CodeRegionInfo::isCall(uint64_t SrcIP, uint64_t TgtIP) const {
  // NOTE: we don't need to normalize the IPs here since we utilize other
  // functions that handle that for us.

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
    return UnknownFI;

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
    return UnknownFI;

  return Result->second;
}

llvm::Optional<FunctionDefinition> CodeRegionInfo::lookup(std::string const& Lib, std::string const& Func) const {
  auto FuncInfo = lookup(Func);

  if (FuncInfo == UnknownFI)
    return llvm::None;

  return FuncInfo->getDefinition(Lib);
}

} // end namespace halo
