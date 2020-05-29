#include "halo/tuner/TuningSection.h"
#include "halo/server/ClientGroup.h"
#include "halo/compiler/ReadELF.h"

namespace halo {


void TuningSection::take_step(GroupState &State) {
  // NOTE: temporary implementation
  if (ShouldCompile) {
    Compiler.enqueueCompilation(Bitcode, RootFunc, Knobs);
    ShouldCompile = false;
  }

  if (!CodeSent) {
    CodeSent = trySendCode(State);
    if (CodeSent)
      logs() << "sent JIT'd code for " << RootFunc << "\n";
  }
}

bool TuningSection::trySendCode(GroupState &State) {
  auto CodeResult = Compiler.dequeueCompilation();
    if (!CodeResult)
      return false; // nothing's ready right now.

  auto CompileOut = std::move(CodeResult.getValue());

  auto MaybeBuf = std::move(CompileOut.Result);
  if (!MaybeBuf)
    return false; // an error etc happened and was logged elsewhere

  std::unique_ptr<llvm::MemoryBuffer> Buf = std::move(MaybeBuf.getValue());
  std::string LibName = CompileOut.UniqueJobName;
  std::string FuncName = RootFunc;

  // tell all clients to load this object file into memory.
  pb::LoadDyLib DylibMsg;
  DylibMsg.set_name(LibName);
  DylibMsg.set_objfile(Buf->getBufferStart(), Buf->getBufferSize());

  // Find all function symbols in the dylib
  auto ELFReadError = readSymbolInfo(Buf->getMemBufferRef(), DylibMsg, FuncName);
  if (ELFReadError)
    fatal_error(std::move(ELFReadError));

  for (auto &Client : State.Clients) {
    auto &DeployedLibs = Client->State.DeployedLibs;

    // send the dylib if the client doesn't have it already
    if (DeployedLibs.count(LibName) == 0) {
      Client->Chan.send_proto(msg::LoadDyLib, DylibMsg);
      DeployedLibs.insert(LibName);
    }

    auto MaybeDef = Client->State.CRI.lookup(CodeRegionInfo::OriginalLib, FuncName);
    if (!MaybeDef)
      fatal_error("client is missing CRI info for an original lib function: " + FuncName);
    auto OriginalDef = MaybeDef.getValue();

    pb::ModifyFunction MF;
    MF.set_name(FuncName);
    MF.set_addr(OriginalDef.Start);
    MF.set_desired_state(pb::FunctionState::REDIRECTED);
    MF.set_other_lib(LibName);
    MF.set_other_name(FuncName);

    Client->Chan.send_proto(msg::ModifyFunction, MF);
  }

  return true;
}

void TuningSection::dump() const {
  clogs() << "TuningSection for " << RootFunc << " {\n";

  clogs() << "ShouldCompile = " << ShouldCompile
          << "\nCodeSent = " << CodeSent
          << "\n";


  clogs() << "}\n\n";
}

llvm::Optional<std::unique_ptr<TuningSection>> TuningSection::Create(
        JSON const& Config, ThreadPool &Pool, CompilationPipeline &Pipeline, Profiler &Profile, llvm::MemoryBuffer &Bitcode) {
  auto MaybeHotNode = Profile.hottestNode();
  if (!MaybeHotNode) return llvm::None;

  auto MaybeAncestor = Profile.getFirstPatchableInContext(MaybeHotNode.getValue());
  if (!MaybeAncestor) return llvm::None;

  std::string PatchableAncestorName = MaybeAncestor.getValue();

  if (!Profile.haveBitcode(PatchableAncestorName))
    return llvm::None;

  return std::unique_ptr<TuningSection>(new TuningSection(Config, Pool, Pipeline, Profile, Bitcode, PatchableAncestorName));
}

} // end namespace