#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/Error.h"

#include "halo/tuner/KnobSet.h"

#include "Logging.h"

#include <memory>
#include <unordered_set>

namespace orc = llvm::orc;

namespace halo {

  class Profiler;

  // Performs the optimization and compilation of a module
  // given a configuration. Thread-safe.
  class CompilationPipeline {
  public:
    using compile_result = std::unique_ptr<llvm::MemoryBuffer>;
    using compile_expected = llvm::Optional<compile_result>;

    CompilationPipeline() {}
    CompilationPipeline(llvm::Triple Triple, llvm::StringRef CPU)
      : Triple(Triple), CPUName(CPU) {}

    // It is crucial that everything passed in here is done by-value, or is a referece to something that is totally immutable.
    // The pipeline is often run in another thread, and we don't want concurrent mutations.
    compile_expected run(llvm::MemoryBuffer &Bitcode, std::string RootFunc, std::unordered_set<std::string> TunedFuncs, KnobSet Knobs) {
      llvm::LLVMContext Cxt; // need a new context for each thread.

      assert(TunedFuncs.find(RootFunc) != TunedFuncs.end() && "root must be in the tuned funcs set!");

      auto MaybeModule = _parseBitcode(Cxt, Bitcode);
      if (!MaybeModule) {
        logs() << "Compilation Error: " << MaybeModule.takeError() << "\n";
        return llvm::None;
      }

      std::unique_ptr<llvm::Module> Module = std::move(MaybeModule.get());

      auto Result = _run(*Module, RootFunc, TunedFuncs, Knobs);
      if (Result)
        return std::move(Result.get());

      logs() << "Compilation Error: " << Result.takeError() << "\n";

      return llvm::None;
    }

    // NOTE: not needed but may want this.
    // compile_result run(orc::ThreadSafeModule &TSM) {
    //   return TSM.withModuleDo([&](llvm::Module &Module) {
    //       return _run(Module);
    //     });
    // }

    // Initializes the given profiler with static program information
    // about the LLVM IR bitcode.
    void analyzeForProfiling(Profiler &, llvm::MemoryBuffer &Bitcode);

    llvm::Triple const& getTriple() const { return Triple; }
    llvm::StringRef getCPUName() const { return CPUName; }

  private:
    llvm::Expected<compile_result> _run(llvm::Module&, std::string const&, std::unordered_set<std::string> const&, KnobSet const&);
    llvm::Expected<std::unique_ptr<llvm::Module>> _parseBitcode(llvm::LLVMContext&, llvm::MemoryBuffer&);

    llvm::Triple Triple;
    std::string CPUName;
  };

} // end namespace halo
