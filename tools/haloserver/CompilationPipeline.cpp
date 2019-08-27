#include "halo/CompilationPipeline.h"

namespace orc = llvm::orc;

namespace halo {

std::unique_ptr<llvm::MemoryBuffer> Compiler::operator()(llvm::Triple const& Triple, llvm::Module &M) {
  orc::JITTargetMachineBuilder JTMB(Triple);
  // TODO: modify the TargetMachine according to the input configuration.
  auto TM = llvm::cantFail(JTMB.createTargetMachine());
  // NOTE: their object cache ignores the TargetMachine's configuration.
  orc::SimpleCompiler C(*TM, /*ObjCache*/ nullptr);
  return C(M);
}

// The complete pipeline
std::unique_ptr<llvm::MemoryBuffer>
  CompilationPipeline::_run(llvm::Module &Module, llvm::StringRef TargetFunc) {

  // Module.print(llvm::outs(), nullptr);

  return Compile(Triple, Module);
}

} // end namespace
