#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/Error.h"

#include "halo/KnobSet.h"

#include <memory>

namespace orc = llvm::orc;

namespace halo {

  // Performs the optimization and compilation of a module
  // given a configuration. Thread-safe.
  class CompilationPipeline {
  public:
    using compile_result = std::unique_ptr<llvm::MemoryBuffer>;
    using compile_expected = llvm::Optional<compile_result>;

    CompilationPipeline() {}
    CompilationPipeline(llvm::Triple Triple, llvm::StringRef CPU)
      : Triple(Triple), CPUName(CPU) {}

    compile_expected run(llvm::MemoryBuffer &Bitcode, llvm::StringRef TargetFunc, KnobSet Knobs) {
      llvm::LLVMContext Cxt; // need a new context for each thread.

      auto MaybeModule = _parseBitcode(Cxt, Bitcode);
      if (!MaybeModule) {
        llvm::outs() << "Compilation Error: " << MaybeModule.takeError() << "\n";
        return llvm::None;
      }

      std::unique_ptr<llvm::Module> Module = std::move(MaybeModule.get());

      auto Result = _run(*Module, TargetFunc);
      if (Result)
        return std::move(Result.get());

      llvm::outs() << "Compilation Error: " << Result.takeError() << "\n";

      return llvm::None;
    }

    // NOTE: not needed but may want this.
    // compile_result run(orc::ThreadSafeModule &TSM) {
    //   return TSM.withModuleDo([&](llvm::Module &Module) {
    //       return _run(Module);
    //     });
    // }

    std::set<std::string> providedFns(llvm::MemoryBuffer &Bitcode);

    llvm::Triple const& getTriple() const { return Triple; }
    llvm::StringRef getCPUName() const { return CPUName; }

  private:
    llvm::Expected<compile_result> _run(llvm::Module&, llvm::StringRef);
    llvm::Expected<std::unique_ptr<llvm::Module>> _parseBitcode(llvm::LLVMContext&, llvm::MemoryBuffer&);

    llvm::Triple Triple;
    std::string CPUName;
  };

} // end namespace halo
