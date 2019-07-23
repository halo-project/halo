
#include "halomon/Error.h"
#include "halomon/Profiler.h"

#include <iostream>
#include <cassert>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "google/protobuf/util/json_util.h"
#include "sanitizer_common/sanitizer_procmaps.h"

#include <elf.h>

namespace san = __sanitizer;
namespace object = llvm::object;
namespace proto = google::protobuf;

namespace halo {

std::string getFunc(const CodeRegionInfo &CRI, uint64_t Addr) {
  auto MaybeInfo = CRI.lookup(Addr);
  if (MaybeInfo)
    return MaybeInfo.getValue()->label();
  return "???";
}

void CodeRegionInfo::loadObjFile(std::string ObjPath) {

  // create new function-offset map
  Data.emplace_back();
  auto &Info = Data.back();
  auto &AddrMap = Info.AddrMap;
  uint64_t &Delta = Info.VMABase;
  auto Index = Data.size() - 1;
  ObjFiles[ObjPath] = Index;

  ///////////
  // initialize the code map

  auto ResOrErr = object::ObjectFile::createObjectFile(ObjPath);
  if (!ResOrErr) halo::fatal_error("error opening object file!");

  object::OwningBinary<object::ObjectFile> OB = std::move(ResOrErr.get());
  object::ObjectFile *Obj = OB.getBinary();

  // find the range of this object file in the process.
  san::uptr VMAStart, VMAEnd;
  bool res = san::GetCodeRangeForFile(ObjPath.data(), &VMAStart, &VMAEnd);
  if (!res) halo::fatal_error("unable to read proc map for VMA range");

  Delta = VMAStart; // Assume PIE is enabled.
  if (auto *ELF = llvm::dyn_cast<object::ELFObjectFileBase>(Obj)) {
    // https://stackoverflow.com/questions/30426383/what-does-pie-do-exactly#30426603
    if (ELF->getEType() == ET_EXEC) {
      Delta = 0; // This is a non-PIE executable.
    }
  }

  auto VMARange =
      icl::right_open_interval<uint64_t>(VMAStart, VMAEnd);
  VMAResolver.insert(std::make_pair(VMARange, Index));

  // Gather function information and place it into the code map.
  for (const object::SymbolRef &Symb : Obj->symbols()) {
    auto MaybeType = Symb.getType();

    if (!MaybeType || MaybeType.get() != object::SymbolRef::Type::ST_Function)
      continue;

    auto MaybeName = Symb.getName();
    auto MaybeAddr = Symb.getAddress();
    uint64_t Size = Symb.getCommonSize();
    if (MaybeName && MaybeAddr && Size > 0) {
      // std::cerr << std::hex
      //           << MaybeName.get().data()
      //           << " at 0x" << MaybeAddr.get()
      //           << " of size 0x" << Size
      //           << "\n";

      uint64_t Start = MaybeAddr.get();
      uint64_t End = Start + Size;
      auto FuncRange = icl::right_open_interval<uint64_t>(Start, End);

      pb::FunctionInfo *newFI = new pb::FunctionInfo();
      newFI->set_label(MaybeName.get());
      newFI->set_start(Start);
      newFI->set_size(Size);
      auto FI = std::shared_ptr<pb::FunctionInfo>(newFI);

      AddrMap.insert(std::make_pair(FuncRange, std::move(FI)));
    }
  }

  // Look for the embedded bitcode
  for (const object::SectionRef &Sec : Obj->sections()) {
    if (!Sec.isBitcode())
      continue;

    auto MaybeData = Sec.getContents();
    if (!MaybeData) halo::fatal_error("unable get bitcode section contents.");

    llvm::SMDiagnostic Err;
    auto MemBuf = llvm::MemoryBuffer::getMemBuffer(MaybeData.get());
    auto Module = llvm::parseIR(*MemBuf, Err, *Info.Cxt);

    if (Module.get() == nullptr) {
      Err.print(ObjPath.c_str(), llvm::errs());
      halo::fatal_error("invalid bitcode.");
    }

    Info.Module = std::move(Module);

    break; // should only be on bitcode section per object file.
  }
}


llvm::Optional<pb::FunctionInfo*> CodeRegionInfo::lookup(uint64_t IP) const {
  size_t Idx = 0;

  // Typically we only have one VMA range that we're tracking,
  // so we avoid the resolver lookup in that case.
  if (Data.size() != 1) {
    auto VMMap = VMAResolver.find(IP);
    if (VMMap == VMAResolver.end())
      return llvm::None;
    Idx = VMMap->second;
  }

  auto &Info = Data[Idx];
  auto &AddrMap = Info.AddrMap;
  IP -= Info.VMABase;

  auto FI = AddrMap.find(IP);
  if (FI == AddrMap.end())
    return llvm::None;

  return FI->second.get();
}

// NOTE: this code should be moved to the halo server
//
// void MonitorState::dump_samples() const {
//   auto &out = std::cerr;
//   for (const pb::RawSample &Sample : RawSamples) {
//     std::string AsJSON;
//     proto::util::JsonPrintOptions Opts;
//     Opts.add_whitespace = true;
//     proto::util::MessageToJsonString(Sample, &AsJSON, Opts);
//     out << AsJSON << "\n---\n";
//
//     out << "CallChain sample len: " << Sample.call_context_size() << "\n";
//     for (const auto &RetAddr : Sample.call_context()) {
//       out << "\t\t " << getFunc(CRI, RetAddr) << " @ 0x"
//                 << std::hex << RetAddr << std::dec << "\n";
//     }
//
//     out << "LBR sample len: " << Sample.branch_size() << "\n";
//     uint64_t Missed = 0, Predicted = 0, Total = 0;
//     for (const auto &BR : Sample.branch()) {
//       Total++;
//       if (BR.mispred()) Missed++;
//       if (BR.predicted()) Predicted++;
//
//       out << std::hex << "\t\t"
//         << getFunc(CRI, BR.from()) << " @ 0x" << BR.from() << " --> "
//         << getFunc(CRI, BR.to())   << " @ 0x" << BR.to()
//         << ", mispred = " << BR.mispred()
//         << ", pred = " << BR.predicted()
//         << std::dec << "\n";
//     }
//   }
// }

}
