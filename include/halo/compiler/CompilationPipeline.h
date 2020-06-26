#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/SmallVectorMemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

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
    CompilationPipeline(llvm::Triple Triple, std::string const& CPU, llvm::StringMap<bool> FeatureMap)
      : Triple(Triple), CPUName(CPU), CPUFeatureMap(FeatureMap) {}

    // This function cleans-up the given module and names the loops in a stable manner.
    // It returns the new bitcode and the number of loop IDs assigned.
    llvm::Optional<std::pair<compile_result, unsigned>> cleanup(llvm::MemoryBuffer &Bitcode, std::string RootFunc, std::unordered_set<std::string> TunedFuncs) {
      llvm::LLVMContext Cxt; // need a new context for each thread.

      assert(TunedFuncs.find(RootFunc) != TunedFuncs.end() && "root must be in the tuned funcs set!");

      auto MaybeModule = _parseBitcode(Cxt, Bitcode);
      if (!MaybeModule) {
        logs() << "Compilation Error: " << MaybeModule.takeError() << "\n";
        return llvm::None;
      }

      std::unique_ptr<llvm::Module> Module = std::move(MaybeModule.get());

      auto Result = _cleanup(*Module, RootFunc, TunedFuncs);
      if (!Result) {
        logs() << "Compilation Error (cleanup): " << Result.takeError() << "\n";
        return llvm::None;
      }

      unsigned NumLoopIDs = Result.get();

      //////
      // we need to serialize this now-cleaned-up module. This involves writing the
      // module to a memory buffer.
      llvm::SmallVector<char, 8192> FileBuf;

      { // dump bitcode to the vector
        llvm::raw_svector_ostream FileStrm(FileBuf);
        llvm::WriteBitcodeToFile(*Module, FileStrm, /*PreserveUseListOrder=*/ true);
      }

      auto MemBuf = std::make_unique<llvm::SmallVectorMemoryBuffer>(std::move(FileBuf));

      return std::make_pair<compile_result, unsigned>(std::move(MemBuf), std::move(NumLoopIDs));
    }


    // This function compiles the given bitcode according to the knob set, and returns an object file.
    //
    // It is crucial that everything passed in here is done by-value, or is a referece to something that is totally immutable.
    // The pipeline is often run in another thread, and we don't want concurrent mutations.
    compile_expected run(llvm::MemoryBuffer &Bitcode, KnobSet Knobs) {
      llvm::LLVMContext Cxt; // need a new context for each thread.

      auto MaybeModule = _parseBitcode(Cxt, Bitcode);
      if (!MaybeModule) {
        logs() << "Compilation Error: " << MaybeModule.takeError() << "\n";
        return llvm::None;
      }

      std::unique_ptr<llvm::Module> Module = std::move(MaybeModule.get());

      auto Result = _run(*Module, Knobs);
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
    llvm::StringMap<bool> const& getCPUFeatures() const { return CPUFeatureMap; }

  private:
    llvm::Expected<unsigned> _cleanup(llvm::Module&, std::string const&, std::unordered_set<std::string> const&);

    llvm::Expected<compile_result> _run(llvm::Module&, KnobSet const&);

    llvm::Expected<std::unique_ptr<llvm::Module>> _parseBitcode(llvm::LLVMContext&, llvm::MemoryBuffer&);

    llvm::Triple Triple;
    std::string CPUName;
    llvm::StringMap<bool> CPUFeatureMap;
  };

} // end namespace halo
