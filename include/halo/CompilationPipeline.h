#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/Error.h"
#include <memory>

namespace orc = llvm::orc;

namespace halo {

  // Performs the optimization and compilation of a module
  // given a configuration. Thread-safe.
  class CompilationPipeline {
  public:
    using compile_result = std::unique_ptr<llvm::MemoryBuffer>;
    using compile_expected = llvm::Expected<compile_result>;

    CompilationPipeline() {}
    CompilationPipeline(llvm::Triple Triple) : Triple(Triple) {}

    compile_expected run(llvm::MemoryBuffer &Bitcode, llvm::StringRef TargetFunc) {
      llvm::LLVMContext Cxt; // need a new context for each thread.

      // NOTE: do NOT use llvm::getLazyBitcodeModule b/c it is not thread-safe!
      auto MaybeModule = llvm::parseBitcodeFile(Bitcode.getMemBufferRef(), Cxt);

      if (!MaybeModule)
        return MaybeModule.takeError();

      std::unique_ptr<llvm::Module> Module = std::move(MaybeModule.get());
      return _run(*Module, TargetFunc);
    }

    // NOTE: not needed but may want this.
    // compile_result run(orc::ThreadSafeModule &TSM) {
    //   return TSM.withModuleDo([&](llvm::Module &Module) {
    //       return _run(Module);
    //     });
    // }

  private:
    compile_expected _run(llvm::Module&, llvm::StringRef);

    llvm::Triple Triple;
  };

} // end namespace halo
