
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
#include "llvm/Transforms/Scalar.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/Transforms/IPO/Attributor.h"
#include "llvm/Transforms/IPO/ForceFunctionAttrs.h"

#include "Logging.h"

using namespace llvm;

// these LLVM command-line options must be declared outside of any namespace
extern cl::opt<AttributorRunOption> AttributorRun;
extern cl::opt<bool> RunPartialInlining;
extern cl::opt<bool> EnableUnrollAndJam;
extern cl::opt<bool> EnableGVNSink;
extern cl::opt<bool> RunNewGVN;
extern cl::opt<bool> EnableGVNHoist;
extern cl::opt<bool> LoopPredicationSkipProfitabilityChecks;

namespace halo {

Expected<std::unique_ptr<MemoryBuffer>> compile(TargetMachine &TM, Module &M) {
  // NOTE: their object cache ignores the TargetMachine's configuration, so we
  // pass in nullptr to disable its use.
  llvm::orc::SimpleCompiler C(TM, /*ObjCache*/ nullptr);
  return C(M);
}


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

    // if the bitcode has already been optimized AOT, let's try to undo loop unrolling
    // so we can redo it possibly better.
    spb::legacy::withPrintAfter(Pr, LegacyPM, "LoopReroll", createLoopRerollPass());

    LegacyPM.run(Module);
  }

  // Add the new PM compatible passes.
  spb::withPrintAfter(Pr, MPM, LinkageFixupPass(RootFunc));
  spb::withPrintAfter(Pr, MPM,
      createModuleToFunctionPassAdaptor(
        createFunctionToLoopPassAdaptor(
          LoopNamerPass(NumLoopIDs))));

  MPM.run(Module, PB.getAnalyses(Triple(Module.getTargetTriple())));

  return Error::success();
}


Error optimize(Module &Module, TargetMachine &TM, KnobSet const& Knobs) {
  bool Pr = true; // printing?
  PipelineTuningOptions PTO; // this is a very nice and extensible way to tune the pipeline.

  // set all the CL options to defaults (from PassManagerBuilder / LLVM) first
  AttributorRun = AttributorRunOption::NONE;
  RunPartialInlining = false;
  EnableUnrollAndJam = false;
  EnableGVNSink = false;
  RunNewGVN = false;
  EnableGVNHoist = false;
  LoopPredicationSkipProfitabilityChecks = false;

  Knobs.lookup<FlagKnob>(named_knob::AttributorEnable).applyFlag([&](bool Flag) {
    if (Flag)
      AttributorRun = AttributorRunOption::ALL;
    else
      AttributorRun = AttributorRunOption::NONE;
  });

  Knobs.lookup<FlagKnob>(named_knob::PartialInlineEnable).applyFlag(RunPartialInlining);
  Knobs.lookup<FlagKnob>(named_knob::UnrollAndJamEnable).applyFlag(EnableUnrollAndJam);
  Knobs.lookup<FlagKnob>(named_knob::GVNSinkEnable).applyFlag(EnableGVNSink);
  Knobs.lookup<FlagKnob>(named_knob::NewGVNEnable).applyFlag(RunNewGVN);
  Knobs.lookup<FlagKnob>(named_knob::NewGVNHoistEnable).applyFlag(EnableGVNHoist);
  Knobs.lookup<FlagKnob>(named_knob::LoopPredicateProfit).applyFlag(LoopPredicationSkipProfitabilityChecks);

  // these options are tuned per-loop, so we need to tell the optimizer that
  // it should always consider it, unless we say otherwise for a particular loop.
  PTO.LoopUnrolling = true;
  PTO.LoopInterleaving = true;
  PTO.LoopVectorization = true;

  // TODO: expose more tuning options in LLVM through the PTO struct or new pases at
  // extension points.

  Knobs.lookup<FlagKnob>(named_knob::SLPVectorizeEnable).applyFlag(PTO.SLPVectorization);

  /// NOTE: IP.OptSizeThreshold and IP.OptMinSizeThreshold
  /// are not currently set. If you end up using / not deleting optsize & minsize attributes
  /// then they may be worth using, though they will have an affect on optimizations other than inlining.

  // internally we default to aggressive inlining thresholds.
  int Threshold = llvm::InlineConstants::OptAggressiveThreshold;
  Knobs.lookup<IntKnob>(named_knob::InlineThreshold).applyScaledVal(Threshold);
  InlineParams IP = llvm::getInlineParams(Threshold);

  /// NOTE: I use to think this was worth tuning, but I believe it is just pessimistically
  /// stopping the cost estimation before it's been fully computed to limit compile time
  /// and thus makes the analysis imprecise.
  //
  /// Specifically, the cost analysis essentially simulates what the resulting function will look like
  /// after inlining + simplification, but the moment the cost exceeds the threshold it
  /// pessimistically stops early if this is set to false, even though it may go back down below
  /// the threshold after simplifications.
  ///
  /// Thus, since compile time doesn't matter for us, maybe we should just always set it to true?
  Knobs.lookup<FlagKnob>(named_knob::InlineFullCost).applyFlag(IP.ComputeFullInlineCost);

  PTO.Inlining = IP;

  // PGOOptions PGO; // TODO: would maybe want to use this later.
  SimplePassBuilder PB(&TM, PTO);


  // internal default
  PassBuilder::OptimizationLevel OptLevel = PassBuilder::OptimizationLevel::O2;
  Knobs.lookup<OptLvlKnob>(named_knob::OptimizeLevel).applyVal(OptLevel);

  ModulePassManager MPM;
  if (OptLevel != PassBuilder::OptimizationLevel::O0) {
    // we only apply optimizations if the level >= 0

    /////
    // The code here is based on llvm::buildPerModuleDefaultPipeline

    // Force any function attributes we want the rest of the pipeline to observe.
    MPM.addPass(ForceFunctionAttrsPass());

    // Pipeline Start EP callbacks would go here
    spb::withPrintAfter(Pr, MPM,
      createModuleToFunctionPassAdaptor(
        createFunctionToLoopPassAdaptor(
          LoopAnnotatorPass(Knobs))));

    // Add the core simplification pipeline.
    MPM.addPass(PB.buildModuleSimplificationPipeline(OptLevel, PassBuilder::ThinLTOPhase::None,
                                                  /*DebugLogging*/ Pr));

    // Now add the optimization pipeline.
    MPM.addPass(PB.buildModuleOptimizationPipeline(OptLevel, /*DebugLogging*/ Pr, /*LTOPreLink*/ false));
  }

  spb::addPrintPass(Pr, MPM, "after optimization pipeline.");
  MPM.run(Module, PB.getAnalyses(Triple(Module.getTargetTriple())));

  return Error::success();
}

// run after optimization to fix-up module before compilation.
Error finalize(Module &Module) {
  bool Pr = false; // printing?
  SimplePassBuilder PB(/*DebugAnalyses*/ false);
  ModulePassManager MPM;

  spb::withPrintAfter(Pr, MPM, ExposeSymbolTablePass());

  MPM.run(Module, PB.getAnalyses(Triple(Module.getTargetTriple())));

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

// based on llvm::codegen::setFunctionAttributes, but we override target-cpu attributes.
void overrideFunctionAttributes(StringRef CPU, StringRef Features, Module &Module) {
  for (Function &F : Module) {
    auto &Ctx = F.getContext();
    AttributeList Attrs = F.getAttributes();
    AttrBuilder NewAttrs;

    if (!CPU.empty())
      NewAttrs.addAttribute("target-cpu", CPU);

    if (!Features.empty()) {
      // Append the command line features to any that are already on the function.
      StringRef OldFeatures = F.getFnAttribute("target-features").getValueAsString();

      if (OldFeatures.empty()) {
        NewAttrs.addAttribute("target-features", Features);

      } else {
        SmallString<256> Appended(OldFeatures);
        Appended.push_back(',');
        Appended.append(Features);
        NewAttrs.addAttribute("target-features", Appended);
      }
    }

    // Let NewAttrs override Attrs.
    F.setAttributes(
        Attrs.addAttributes(Ctx, AttributeList::FunctionIndex, NewAttrs));
  }
}

// The complete pipeline
Expected<CompilationPipeline::compile_result>
  CompilationPipeline::_run(Module &Module, KnobSet const& Knobs) {

  orc::JITTargetMachineBuilder JTMB(Triple);

  Knobs.lookup<FlagKnob>(named_knob::NativeCPU).applyFlag([&](bool Flag) {
    if (Flag) {
      JTMB.setCPU(getCPUName().str());
      SubtargetFeatures &Features = JTMB.getFeatures();
      for (auto &F : getCPUFeatures())
        Features.AddFeature(F.first(), F.second);

      overrideFunctionAttributes(getCPUName(), Features.getString(), Module);
    }
  });

  // default llc optimization level, if nothing is set.
  CodeGenOpt::Level CodeGenLevel = CodeGenOpt::Aggressive;
  Knobs.lookup<OptLvlKnob>(named_knob::CodegenLevel).applyCodegenLevel(CodeGenLevel);
  JTMB.setCodeGenOptLevel(CodeGenLevel);

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

  // Let's not use machine outliner. I don't think my infrastructure can determine the outlined function's
  // position and size. The call-graph would also need to be updated dynamically to aid the BTB entries in samples
  // too. I think this would just lead to inaccurate IPC measurements due to thrown-out samples.
  //
  // Knobs.lookup<FlagKnob>(named_knob::MachineOutline).applyFlag([&](bool Flag) { TO.EnableMachineOutliner = Flag; });
  TO.EnableMachineOutliner = false;

  // It doesn't make sense to have this ever be off.
  // Knobs.lookup<FlagKnob>(named_knob::GuaranteeTCO).applyFlag([&](bool Flag) { TO.GuaranteedTailCallOpt = Flag; });
  TO.GuaranteedTailCallOpt = true;

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
  llvm::Triple TheTriple(Twine(Module->getTargetTriple()));

  MPM.addPass(ProgramInfoPass(Profile));

  MPM.run(*Module, PB.getAnalyses(TheTriple));
}

} // end namespace
