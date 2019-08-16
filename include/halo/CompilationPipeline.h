#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/IR/LLVMContext.h"
#include <memory>

namespace orc = llvm::orc;

namespace halo {
  class CompilationPipeline;

  // A thread-safe compiler functor.
  class Compiler {
  private:
    friend class CompilationPipeline;

    // Compile the given module to an in-memory object file.
    std::unique_ptr<llvm::MemoryBuffer> operator()(llvm::Triple const& Triple, llvm::Module &M) {
      orc::JITTargetMachineBuilder JTMB(Triple);
      // TODO: modify the TargetMachine according to the input configuration.
      auto TM = llvm::cantFail(JTMB.createTargetMachine());
      // NOTE: their object cache ignores the TargetMachine's configuration.
      orc::SimpleCompiler C(*TM, /*ObjCache*/ nullptr);
      return C(M);
    }

  };

  // Coordinates the parallel optimization and compilation of a module
  // given the configuration. Thread-safe.
  class CompilationPipeline {
  public:
    using compile_result = std::unique_ptr<llvm::MemoryBuffer>;
    using compile_expected = llvm::Expected<compile_result>;

    CompilationPipeline() {}
    CompilationPipeline(llvm::Triple Triple) : Triple(Triple) {}

    compile_expected run(llvm::MemoryBuffer &Bitcode) {
      llvm::LLVMContext Cxt; // need a new context for each thread.
      // NOTE: llvm::getLazyBitcodeModule is NOT thread-safe!
      auto MaybeModule = llvm::parseBitcodeFile(Bitcode.getMemBufferRef(), Cxt);

      if (!MaybeModule)
        return MaybeModule.takeError();

      std::unique_ptr<llvm::Module> Module = std::move(MaybeModule.get());
      return _run(*Module);
    }

    compile_result run(orc::ThreadSafeModule &TSM) {
      return TSM.withModuleDo([&](llvm::Module &Module) {
          return _run(Module);
        });
    }

  private:

    // The complete pipeline
    std::unique_ptr<llvm::MemoryBuffer> _run(llvm::Module &Module) {

      // Module.print(llvm::outs(), nullptr);

      return Compile(Triple, Module);
    }

    llvm::Triple Triple;
    Compiler Compile;
  };

} // end namespace halo
