
#include "halo/CompilationPipeline.h"
#include "halo/LinkageFixupPass.h"
#include "halo/LoopNamerPass.h"
#include "halo/SimplePassBuilder.h"
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

// obtain the transitive closure of all directly-called functions starting
// from the given function.
Expected<std::vector<GlobalValue*>> getObviousCallees(Module &Module, StringRef Name) {
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

    for (auto &Blk : *Fn) {
      for (auto &Inst : Blk) {
        CallBase *Call = dyn_cast_or_null<CallBase>(&Inst);
        if (!Call)
          continue;
        Function *Callee = Call->getCalledFunction();
        if (!Callee)
          continue;
        if (Callee->isDeclaration() || Deps.count(Callee))
          continue;

        Deps.insert(Callee);
        Work.push_back(Callee);
      }
    }
  }

  return Deps.takeVector();
}

Error cleanup(Module &Module, StringRef TargetFunc) {
  bool Pr = true; // printing?
  SimplePassBuilder PB(/*DebugAnalyses*/ false);
  ModulePassManager MPM;

  StripDebugInfo(Module); // NOTE for now just drop all debug info.

  ///////
  // the process of cleaning up the module in prep for JIT compilation is
  // very similar to what happens in the llvm-extract tool.

  auto MaybeDeps = getObviousCallees(Module, TargetFunc);
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

Error optimize(Module &Module, TargetMachine &TM) {
  PipelineTuningOptions PTO; // this is a very nice and extensible way to tune the pipeline.
  // PGOOptions PGO; // TODO: would want to use this later.
  SimplePassBuilder PB(&TM, PTO);

  ModulePassManager MPM =
        PB.buildPerModuleDefaultPipeline(PassBuilder::O3,
                                          /*Debug*/ false,
                                          /*LTOPreLink*/ false);

  pb::addPrintPass(true, MPM, "after optimization pipeline.");
  MPM.run(Module, PB.getAnalyses());

  return Error::success();
}

// The complete pipeline
Expected<CompilationPipeline::compile_result>
  CompilationPipeline::_run(Module &Module, StringRef TargetFunc) {

  llvm::orc::JITTargetMachineBuilder JTMB(Triple);
  auto MaybeTM = JTMB.createTargetMachine();
  if (!MaybeTM)
    return MaybeTM.takeError();

  auto TM = std::move(MaybeTM.get());
  // TODO: modify the TargetMachine according to the input configuration.

  auto CleanupErr = cleanup(Module, TargetFunc);
  if (CleanupErr)
    return CleanupErr;


  auto OptErr = optimize(Module, *TM);
  if (OptErr)
    return OptErr;

  // Module.print(outs(), nullptr);

  return compile(*TM, Module);
}

} // end namespace
