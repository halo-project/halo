
#include "halo/CompilationPipeline.h"
#include "halo/LinkageFixupPass.h"
#include "halo/LoopNamerPass.h"
#include "halo/SimplePassBuilder.h"
#include "halo/NamedKnobs.h"

#include "llvm/Transforms/IPO.h"
#include "llvm/IR/DebugInfo.h"

#include "Logging.h"

using namespace llvm;

namespace halo {

Expected<std::unique_ptr<MemoryBuffer>> compile(TargetMachine &TM, Module &M) {
  // NOTE: their object cache ignores the TargetMachine's configuration, so we
  // pass in nullptr to disable its use.
  llvm::orc::SimpleCompiler C(TM, /*ObjCache*/ nullptr);
  return C(M);
}

// obtain the transitive closure of all functions needed by the given function.
Expected<std::vector<GlobalValue*>> findRequiredFuncs(Module &Module, StringRef Name) {
  Function* RootFn = Module.getFunction(Name);
  if (RootFn == nullptr)
    return makeError("target function " + Name + " is not in module!");

  if (RootFn->isDeclaration())
    return makeError("target global " + Name + " is not a defined function.");

  // determine the transitive closure of dependencies
  SetVector<GlobalValue*> Deps;
  std::vector<Function*> Work;
  Work.push_back(RootFn);

  while (!Work.empty()) {
    Function *Fn = &*Work.back();
    Work.pop_back();
    Deps.insert(Fn);

    // search each instructino to find uses of functions.
    for (auto &Blk : *Fn)
      for (auto &Inst : Blk)
        for (Value* Op : Inst.operands())
          if (User* OpUse = dyn_cast_or_null<User>(Op))
            if (Function* NewFn = dyn_cast_or_null<Function>(OpUse))
              if (Deps.count(NewFn) == 0)
                  Work.push_back(NewFn);
  }

  return Deps.takeVector();
}

Error cleanup(Module &Module, StringRef TargetFunc) {
  bool Pr = false; // printing?
  SimplePassBuilder PB(/*DebugAnalyses*/ false);
  ModulePassManager MPM;

  StripDebugInfo(Module); // NOTE for now just drop all debug info.

  ///////
  // the process of cleaning up the module in prep for JIT compilation is
  // very similar to what happens in the llvm-extract tool.

  auto MaybeDeps = findRequiredFuncs(Module, TargetFunc);
  if (!MaybeDeps)
    return MaybeDeps.takeError();

  { // some passes haven't been updated to use NewPM :(
    legacy::PassManager LegacyPM;
    pb::legacy::addPrintPass(Pr, LegacyPM, "START of cleanup");

    // slim down the module
    pb::legacy::withPrintAfter(Pr, LegacyPM,
            "GVExtract", createGVExtractionPass(MaybeDeps.get()));

    pb::legacy::withPrintAfter(Pr, LegacyPM,
            "GlobalDCE", createGlobalDCEPass()); // Delete unreachable globals

    pb::legacy::withPrintAfter(Pr, LegacyPM,
            "StripDeadDebug", createStripDeadDebugInfoPass()); // Remove dead debug info

    pb::legacy::withPrintAfter(Pr, LegacyPM,
            "StripDeadProto", createStripDeadPrototypesPass()); // Remove dead func decls

    LegacyPM.run(Module);
  }

  pb::withPrintAfter(Pr, MPM, LinkageFixupPass(TargetFunc));
  pb::withPrintAfter(Pr, MPM,
      createModuleToFunctionPassAdaptor(
        createFunctionToLoopPassAdaptor(
          LoopNamerPass())));

  MPM.run(Module, PB.getAnalyses());

  return Error::success();
}

Error optimize(Module &Module, TargetMachine &TM, KnobSet const& Knobs) {
  bool Pr = false; // printing?
  PipelineTuningOptions PTO; // this is a very nice and extensible way to tune the pipeline.
  // PGOOptions PGO; // TODO: would want to use this later.
  SimplePassBuilder PB(&TM, PTO);

  auto OptLevel = Knobs.lookup<OptLvlKnob>(named_knob::OptimizeLevel).getVal();
  ModulePassManager MPM =
        PB.buildPerModuleDefaultPipeline(OptLevel,
                                          /*Debug*/ false,
                                          /*LTOPreLink*/ false);

  pb::addPrintPass(Pr, MPM, "after optimization pipeline.");
  MPM.run(Module, PB.getAnalyses());

  return Error::success();
}

// The complete pipeline
Expected<CompilationPipeline::compile_result>
  CompilationPipeline::_run(Module &Module, StringRef TargetFunc, KnobSet const& Knobs) {

  llvm::orc::JITTargetMachineBuilder JTMB(Triple);
  auto MaybeTM = JTMB.createTargetMachine();
  if (!MaybeTM)
    return MaybeTM.takeError();

  auto TM = std::move(MaybeTM.get());
  TargetOptions TO = TM->DefaultOptions; // grab defaults

  TO.EnableIPRA = Knobs.lookup<FlagKnob>(named_knob::IPRA).getFlag();

  TO.EnableFastISel = Knobs.lookup<FlagKnob>(named_knob::FastISel).getFlag();
  TO.EnableGlobalISel = Knobs.lookup<FlagKnob>(named_knob::GlobalISel).getFlag();

  TO.EnableMachineOutliner = Knobs.lookup<FlagKnob>(named_knob::MachineOutline).getFlag();
  TO.GuaranteedTailCallOpt = Knobs.lookup<FlagKnob>(named_knob::GuaranteeTCO).getFlag();

  TM->Options = TO; // save the options


  auto CleanupErr = cleanup(Module, TargetFunc);
  if (CleanupErr)
    return CleanupErr;


  auto OptErr = optimize(Module, *TM, Knobs);
  if (OptErr)
    return OptErr;

  // Module.print(outs(), nullptr);

  return compile(*TM, Module);
}

llvm::Expected<std::unique_ptr<llvm::Module>>
  CompilationPipeline::_parseBitcode(llvm::LLVMContext &Cxt, llvm::MemoryBuffer &Bitcode) {
    // NOTE: do NOT use llvm::getLazyBitcodeModule b/c it is not thread-safe!
    return llvm::parseBitcodeFile(Bitcode.getMemBufferRef(), Cxt);
  }

std::set<std::string>
  CompilationPipeline::providedFns(llvm::MemoryBuffer &Bitcode) {
  llvm::LLVMContext Cxt;
  std::set<std::string> Provided;

  auto MaybeModule = _parseBitcode(Cxt, Bitcode);
  if (!MaybeModule) {
    log() << MaybeModule.takeError() << "\n";
    warning("Error parsing bitcode!\n", true);
    return Provided;
  }

  auto Module = std::move(MaybeModule.get());

  for (auto &Fn : Module->functions()) {
    if (Fn.isDeclaration())
      continue;

    Provided.insert(Fn.getName());
  }

  return Provided;
}

} // end namespace
