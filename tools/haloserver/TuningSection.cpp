#include "halo/tuner/TuningSection.h"
#include "halo/tuner/CompileOnceTuningSection.h"
#include "halo/tuner/AdaptiveTuningSection.h"
#include "halo/server/ClientGroup.h"
#include "halo/compiler/ReadELF.h"
#include "halo/tuner/NamedKnobs.h"
#include "halo/nlohmann/util.hpp"

#include "llvm/Support/CommandLine.h"

namespace cl = llvm::cl;

static cl::opt<halo::Strategy::Kind> CL_Strategy(
  "halo-strategy",
  cl::desc("The strategy to use when optimizing hot code regions."),
  cl::init(halo::Strategy::Adaptive),
  cl::values(clEnumValN(halo::Strategy::Adaptive, "adapt", "The main adaptive tuning strategy."),
             clEnumValN(halo::Strategy::JitOnce, "jit", "Does not tune. Instead compiles the hot code once at high default optimization.")));

namespace halo {


llvm::Optional<std::unique_ptr<TuningSection>> TuningSection::Create(TuningSectionInitializer TSI) {
  auto MaybeHotNode = TSI.Profile.hottestNode();
  if (!MaybeHotNode){
    info("TuningSection::Create -- no suitable hottest node.");
    return llvm::None;
  }

  auto MaybeAncestor = TSI.Profile.findSuitableTuningRoot(MaybeHotNode.getValue());
  if (!MaybeAncestor) {
    info("TuningSection::Create -- no suitable tuning root.");
    return llvm::None;
  }

  std::string PatchableAncestorName = MaybeAncestor.getValue();

  if (!TSI.Profile.haveBitcode(PatchableAncestorName)) {
    info("TuningSection::Create -- no bitcode available for tuning root.");
    return llvm::None;
  }

  TuningSection *TS = nullptr;

  switch (CL_Strategy) {
    case Strategy::Adaptive: {
      TS = new AdaptiveTuningSection(TSI, PatchableAncestorName);
    } break;

    case Strategy::JitOnce: {
      TS = new CompileOnceTuningSection(TSI, PatchableAncestorName);
    } break;

    default: fatal_error("unhandled tuning section strategy");
  };

  assert(TS != nullptr);

  return std::unique_ptr<TuningSection>(TS);
}



TuningSection::TuningSection(TuningSectionInitializer TSI, std::string RootFunc)
    : FnGroup(RootFunc), Compiler(TSI.CompilerPool, TSI.Pipeline), Profile(TSI.Profile) {
  ////////////
  // Choose the set of all funcs in this tuning section.

  // start off with all functions reachable according to the call-graph
  auto Reachable = Profile.getCallGraph().allReachable(RootFunc);

  // filter down that set to just those for which we have bitcode
  for (auto const& Func : Reachable)
    if (Func.HaveBitcode)
      FnGroup.AllFuncs.insert(Func.Name);

  // now, we clean-up the original bitcode to only include those functions
  auto MaybeResult = TSI.Pipeline.cleanup(TSI.OriginalBitcode, FnGroup.Root, FnGroup.AllFuncs);
  if (!MaybeResult)
    fatal_error("couldn't clean-up bitcode for tuning section!");

  auto Result = std::move(MaybeResult.getValue());
  Bitcode = std::move(Result.first);
  unsigned MaxLoopID = Result.second;

  /////
  // Finally, we can initialize the knobs for this tuning section
  KnobSet::InitializeKnobs(TSI.Config, BaseKnobs, MaxLoopID);

  ///////
  // Now, we initialize the OriginalLibKnobs as a subset of the BaseKnobs
  // We can only do this if the original build setting is in range of
  // the knob specified in the JSON file.

  auto OrigOptLvl = TSI.OriginalSettings.OptLvl;
  auto OK = std::make_unique<OptLvlKnob>(BaseKnobs.lookup<OptLvlKnob>(named_knob::OptimizeLevel));
  if (OK->getMin() <= OrigOptLvl && OrigOptLvl <= OK->getMax()) {
    OK->setVal(OrigOptLvl);
    OriginalLibKnobs.insert(std::move(OK));
  }

}



void TuningSection::sendLib(GroupState &State, CodeVersion const& CV) {
  if (CV.isBroken()) {
    warning("trying to send broken lib.");
    return;
  }

  if(CV.isOriginalLib())
    return; // nothing to do!

  std::unique_ptr<llvm::MemoryBuffer> const& Buf = CV.getObjectFile();
  std::string LibName = CV.getLibraryName();
  std::string FuncName = FnGroup.Root;

  // tell all clients to load this object file into memory.
  pb::LoadDyLib DylibMsg;
  DylibMsg.set_name(LibName);
  DylibMsg.set_objfile(Buf->getBufferStart(), Buf->getBufferSize());

  // Find all function symbols in the dylib
  auto ELFReadError = readSymbolInfo(Buf->getMemBufferRef(), DylibMsg, FuncName);
  if (ELFReadError)
    fatal_error(std::move(ELFReadError));

  for (auto &Client : State.Clients)
    Client->send_library(Client->State, DylibMsg);
}

void TuningSection::redirectTo(GroupState &State, CodeVersion const& CV) {
  if (CV.isBroken()) {
    warning("trying to redirect to broken lib.");
    return;
  }

  std::string LibName = CV.getLibraryName();
  std::string FuncName = FnGroup.Root;

  // NOTE: this is _partially_ initialized. we let the client modify the addr field
  // before it sends it off (if needed).
  pb::ModifyFunction MF;
  MF.set_name(FuncName);
  MF.set_desired_state(pb::FunctionState::REDIRECTED);
  MF.set_other_lib(LibName);
  MF.set_other_name(FuncName);

  for (auto &Client : State.Clients)
    Client->redirect_to(Client->State, MF);
}

} // end namespace