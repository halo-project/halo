
#include "halo/compiler/CompilationPipeline.h"
#include "halo/compiler/ExposeSymbolTablePass.h"
#include "halo/compiler/LinkageFixupPass.h"
#include "halo/compiler/LoopNamerPass.h"
#include "halo/compiler/LoopAnnotatorPass.h"
#include "halo/compiler/SimplePassBuilder.h"
#include "halo/compiler/Profiler.h"
#include "halo/compiler/ProgramInfoPass.h"
#include "halo/tuner/NamedKnobs.h"

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

// TODO: make this take a Module const& because it doesn't mutate the module.
Expected<std::vector<GlobalValue*>> findRequiredFuncs(Module &Module, std::unordered_set<std::string> const& TunedFuncs) {
  SetVector<Function*> Work;

  // add tuned funcs to work queue
  for (auto const& Name : TunedFuncs) {
    Function* Fn = Module.getFunction(Name);
    if (Fn == nullptr)
      return makeError("target function " + Name + " is not in module!");

    if (Fn->isDeclaration())
      return makeError("target global " + Name + " is not a defined function.");

    Work.insert(Fn);
  }

  // determine the transitive closure of dependencies by checking
  // the body of all functions for _any_ uses of _any_ function.
  // This catches dependencies that are missed by call-graph analysis
  // in order to satisfy the dynamic linker. The biggest culprit of functions
  // missed by the analysis are function pointers that are used as a value.
  SetVector<GlobalValue*> Deps;
  while (!Work.empty()) {
    Function *Fn = &*Work.back();
    Work.pop_back();
    Deps.insert(Fn);

    // search each instruction in a breadth-first manner to find any uses of functions.
    for (auto &Blk : *Fn)
      for (auto &Inst : Blk)
        for (Value* Op : Inst.operands())
          if (User* OpUse = dyn_cast_or_null<User>(Op))
            if (Function* NewFn = dyn_cast_or_null<Function>(OpUse))
              if (Deps.count(NewFn) == 0 && Work.count(NewFn) == 0) {
                  auto const& Name = NewFn->getName();
                  if (TunedFuncs.count(Name.str()) == 0)
                    logs() << "findRequiredFuncs missing dependency: " << Name << "\n"; // just a note; nothing bad about this!
                  Work.insert(NewFn);
              }
  }

  return Deps.takeVector();
}

Error doCleanup(Module &Module, std::string const& RootFunc, std::unordered_set<std::string> const& TunedFuncs, unsigned &NumLoopIDs) {
  bool Pr = false; // printing?
  SimplePassBuilder PB(/*DebugAnalyses*/ false);
  ModulePassManager MPM;

  StripDebugInfo(Module); // NOTE for now just drop all debug info.

  ///////
  // the process of cleaning up the module in prep for JIT compilation is
  // very similar to what happens in the llvm-extract tool.

  auto MaybeDeps = findRequiredFuncs(Module, TunedFuncs);
  if (!MaybeDeps)
    return MaybeDeps.takeError();

  // Add the passes (legacy)

  { // some passes haven't been updated to use NewPM :(
    legacy::PassManager LegacyPM;
    spb::legacy::addPrintPass(Pr, LegacyPM, "START of cleanup");

    // slim down the module
    spb::legacy::withPrintAfter(Pr, LegacyPM,
            "GVExtract", createGVExtractionPass(MaybeDeps.get()));

    spb::legacy::withPrintAfter(Pr, LegacyPM,
            "GlobalDCE", createGlobalDCEPass()); // Delete unreachable globals

    spb::legacy::withPrintAfter(Pr, LegacyPM,
            "StripDeadDebug", createStripDeadDebugInfoPass()); // Remove dead debug info

    spb::legacy::withPrintAfter(Pr, LegacyPM,
            "StripDeadProto", createStripDeadPrototypesPass()); // Remove dead func decls

    LegacyPM.run(Module);
  }

  // Add the new PM compatible passes.
  spb::withPrintAfter(Pr, MPM, LinkageFixupPass(RootFunc));
  spb::withPrintAfter(Pr, MPM,
      createModuleToFunctionPassAdaptor(
        createFunctionToLoopPassAdaptor(
          LoopNamerPass(NumLoopIDs))));

  MPM.run(Module, PB.getAnalyses());

  return Error::success();
}


/// This function will apply annotations to the named loops in the module, according to
/// the knob configuration.
Error annotateLoops(Module &Module, KnobSet const& Knobs) {
  bool Pr = false; // printing?
  SimplePassBuilder PB(/*DebugAnalyses*/ false);
  ModulePassManager MPM;

  spb::withPrintAfter(Pr, MPM,
      createModuleToFunctionPassAdaptor(
        createFunctionToLoopPassAdaptor(
          LoopAnnotatorPass(Knobs))));

  MPM.run(Module, PB.getAnalyses());

  return Error::success();
}


Error optimize(Module &Module, TargetMachine &TM, KnobSet const& Knobs) {
  bool Pr = false; // printing?
  PipelineTuningOptions PTO; // this is a very nice and extensible way to tune the pipeline.

  /// NOTE: IP.OptSizeThreshold and IP.OptMinSizeThreshold
  /// are not currently set. If you end up using / not deleting optsize & minsize attributes
  /// then they may be worth using, though they will have an affect on optimizations other than inlining.
  InlineParams IP;
  IP.DefaultThreshold =
    Knobs.lookup<IntKnob>(named_knob::InlineThreshold).getScaledVal();

  IP.HintThreshold =
    Knobs.lookup<IntKnob>(named_knob::InlineThresholdHint).getScaledVal();

  IP.ColdThreshold =
    Knobs.lookup<IntKnob>(named_knob::InlineThresholdCold).getScaledVal();

  IP.HotCallSiteThreshold =
    Knobs.lookup<IntKnob>(named_knob::InlineThresholdHotSite).getScaledVal();

  IP.LocallyHotCallSiteThreshold =
    Knobs.lookup<IntKnob>(named_knob::InlineThresholdLocalHotSite).getScaledVal();

  IP.ColdCallSiteThreshold =
    Knobs.lookup<IntKnob>(named_knob::InlineThresholdColdSite).getScaledVal();

  /// NOTE: It may not even bet worthwhile to tune this? I believe it is just pessimistically
  /// stopping the cost estimation before it's been fully computed to limit compile time.
  /// Since the cost analysis essentially simulates what the resulting function will look like
  /// after inlining + simplification.
  /// Thus, since compile time doesn't matter for us with such small snippets of code, we may
  /// want to just fix it to be `true`
  Knobs.lookup<FlagKnob>(named_knob::InlineFullCost).applyFlag(IP.ComputeFullInlineCost);

  PTO.Inlining = IP;

  // PGOOptions PGO; // TODO: would maybe want to use this later.
  SimplePassBuilder PB(&TM, PTO);

  auto OptLevel = Knobs.lookup<OptLvlKnob>(named_knob::OptimizeLevel).getVal();

  ModulePassManager MPM;
  if (OptLevel != PassBuilder::OptimizationLevel::O0) {
    // we only apply optimizations if the level >= 0
    auto LoopAnnotateErr = annotateLoops(Module, Knobs);
    if (LoopAnnotateErr)
      return LoopAnnotateErr;

    MPM = PB.buildPerModuleDefaultPipeline(OptLevel, /*Debug*/ false, /*LTOPreLink*/ false);
  }

  spb::addPrintPass(Pr, MPM, "after optimization pipeline.");
  MPM.run(Module, PB.getAnalyses());

  return Error::success();
}

// run after optimization to fix-up module before compilation.
Error finalize(Module &Module) {
  bool Pr = false; // printing?
  SimplePassBuilder PB(/*DebugAnalyses*/ false);
  ModulePassManager MPM;

  spb::withPrintAfter(Pr, MPM, ExposeSymbolTablePass());

  MPM.run(Module, PB.getAnalyses());

  return Error::success();
}

// the clean-up step. It mutates the module passed in to clean it up.
Expected<unsigned>
  CompilationPipeline::_cleanup(Module &Module, std::string const& RootFunc, std::unordered_set<std::string> const& TunedFuncs) {

  unsigned NumLoopIDs = 0;
  auto CleanupErr = doCleanup(Module, RootFunc, TunedFuncs, NumLoopIDs);
  if (CleanupErr)
    return CleanupErr;

  return NumLoopIDs;
}



// The complete pipeline
Expected<CompilationPipeline::compile_result>
  CompilationPipeline::_run(Module &Module, KnobSet const& Knobs) {

  llvm::orc::JITTargetMachineBuilder JTMB(Triple);

  JTMB.setCodeGenOptLevel(Knobs.lookup<OptLvlKnob>(named_knob::CodegenLevel).asCodegenLevel());

  auto MaybeTM = JTMB.createTargetMachine();
  if (!MaybeTM)
    return MaybeTM.takeError();

  auto TM = std::move(MaybeTM.get());
  TargetOptions TO = TM->DefaultOptions; // grab defaults

  Knobs.lookup<FlagKnob>(named_knob::IPRA).applyFlag([&](bool Flag) { TO.EnableIPRA = Flag; });

  Knobs.lookup<FlagKnob>(named_knob::FastISel).applyFlag([&](bool Flag) { TO.EnableFastISel = Flag; });

  // NOTE: global isel doesn't seem to be ready for use yet; it creates malformed call instructions sometimes.
  // TO.EnableGlobalISel = Knobs.lookup<FlagKnob>(named_knob::GlobalISel).getFlag();
  TO.EnableGlobalISel = false;

  Knobs.lookup<FlagKnob>(named_knob::MachineOutline).applyFlag([&](bool Flag) { TO.EnableMachineOutliner = Flag; });
  Knobs.lookup<FlagKnob>(named_knob::GuaranteeTCO).applyFlag([&](bool Flag) { TO.GuaranteedTailCallOpt = Flag; });

  TM->Options = TO; // save the options

  auto OptErr = optimize(Module, *TM, Knobs);
  if (OptErr)
    return OptErr;

  auto FinalErr = finalize(Module);
  if (FinalErr)
    return FinalErr;

  // Module.print(logs(), nullptr);

  return compile(*TM, Module);
}

llvm::Expected<std::unique_ptr<llvm::Module>>
  CompilationPipeline::_parseBitcode(llvm::LLVMContext &Cxt, llvm::MemoryBuffer &Bitcode) {
    // NOTE: do NOT use llvm::getLazyBitcodeModule b/c it is not thread-safe!
    return llvm::parseBitcodeFile(Bitcode.getMemBufferRef(), Cxt);
  }

void CompilationPipeline::analyzeForProfiling(Profiler &Profile, llvm::MemoryBuffer &Bitcode) {
  llvm::LLVMContext Cxt;

  // Parse the bitcode
  auto MaybeModule = _parseBitcode(Cxt, Bitcode);
  if (!MaybeModule) {
    logs() << MaybeModule.takeError() << "\n";
    fatal_error("Error parsing bitcode!\n");
  }

  auto Module = std::move(MaybeModule.get());

  // Populate the profiler with static program information.
  SimplePassBuilder PB;
  ModulePassManager MPM;

  MPM.addPass(ProgramInfoPass(Profile));
  MPM.run(*Module, PB.getAnalyses());
}

} // end namespace
