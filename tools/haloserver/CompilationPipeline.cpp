
#include "halo/CompilationPipeline.h"
#include "halo/ExternalizeGlobalsPass.h"
#include "halo/LoopNamerPass.h"
#include "halo/SimplePassBuilder.h"
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
  SimplePassBuilder PB;
  llvm::ModulePassManager MPM;

  MPM.addPass(ExternalizeGlobalsPass());
  MPM.addPass(llvm::createModuleToFunctionPassAdaptor(
                llvm::createFunctionToLoopPassAdaptor(
                  LoopNamerPass())));

  MPM.run(Module, PB.getAnalyses());

  return llvm::Error::success();
}

llvm::Error optimize(llvm::Module &Module, llvm::TargetMachine &TM) {
  llvm::PipelineTuningOptions PTO; // this is a very nice and extensible way to tune the pipeline.
  // llvm::PGOOptions PGO; // TODO: would want to use this later.
  SimplePassBuilder PB(&TM, PTO);

  llvm::ModulePassManager MPM =
        PB.buildPerModuleDefaultPipeline(llvm::PassBuilder::O3,
                                          /*Debug*/ false,
                                          /*LTOPreLink*/ false);
  MPM.run(Module, PB.getAnalyses());

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
