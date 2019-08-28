
#include "halo/CompilationPipeline.h"
#include "halo/ExternalizeGlobalsPass.h"
#include "halo/LoopNamerPass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/Error.h"

namespace orc = llvm::orc;

namespace halo {

llvm::Expected<std::unique_ptr<llvm::MemoryBuffer>> compile(llvm::TargetMachine &TM, llvm::Module &M) {
  // NOTE: their object cache ignores the TargetMachine's configuration, so we
  // pass in nullptr to disable its use.
  orc::SimpleCompiler C(TM, /*ObjCache*/ nullptr);
  return C(M);
}

llvm::Error cleanup(llvm::Module &Module, llvm::StringRef TargetFunc) {
  return llvm::Error::success(); // FIXME: this causes segfaults. find out why!

  bool DebugPM = false;
  llvm::ModuleAnalysisManager MAM(DebugPM);
  llvm::ModulePassManager MPM(DebugPM);

  MPM.addPass(ExternalizeGlobalsPass());
  MPM.addPass(llvm::createModuleToFunctionPassAdaptor(
                llvm::createFunctionToLoopPassAdaptor(
                  LoopNamerPass())));

  MPM.run(Module, MAM);

  return llvm::Error::success();
}

llvm::Error optimize(llvm::Module &Module, llvm::TargetMachine &TM) {
  // See llvm/tools/opt/NewPMDriver.cpp for reference.

  bool DebugPM = false;
  llvm::PipelineTuningOptions PTO; // this is a very nice and extensible way to tune the pipeline.
  // llvm::PGOOptions PGO;
  llvm::PassInstrumentationCallbacks PIC;
  llvm::PassBuilder PB(&TM, PTO, llvm::None, &PIC);

  llvm::AAManager AA;
  llvm::LoopAnalysisManager LAM(DebugPM);
  llvm::FunctionAnalysisManager FAM(DebugPM);
  llvm::CGSCCAnalysisManager CGAM(DebugPM);
  llvm::ModuleAnalysisManager MAM(DebugPM);

  // Register the AA manager first so that our version is the one used.
  FAM.registerPass([&] { return std::move(AA); });

  // Register all the basic analyses with the managers.
  PB.registerModuleAnalyses(MAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

  llvm::ModulePassManager MPM =
        PB.buildPerModuleDefaultPipeline(llvm::PassBuilder::O3, DebugPM,
                                          /*LTOPreLink*/ false);
  MPM.run(Module, MAM);

  return llvm::Error::success();
}

// The complete pipeline
llvm::Expected<CompilationPipeline::compile_result>
  CompilationPipeline::_run(llvm::Module &Module, llvm::StringRef TargetFunc) {

  orc::JITTargetMachineBuilder JTMB(Triple);
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

  // Module.print(llvm::outs(), nullptr);

  return compile(*TM, Module);
}

} // end namespace
