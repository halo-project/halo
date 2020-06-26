#pragma once

#include "llvm/ADT/Optional.h"

// for new PM
#include "halo/compiler/PrintIRPass.h"
#include "llvm/Passes/PassBuilder.h"

// for legacy PM
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/IRPrintingPasses.h"

#include <utility>

namespace halo {

  /// This exists because LLVM's new pass manager is incredibly annoying to use.
  /// All of the steps you need to do in order to correctly build
  /// a small custom pass pipeline is baked into the implementation of the
  /// PassBuilder object, which doesn't provide sufficient flexibility.
  /// Namely, all of the methods to generate a pipeline say
  /// "O0 is not a valid option", so you're stuck with what PassBuilder thinks
  /// you should run on the module.
  ///
  /// The purpose of this class is to hide the mess of setting up analysis
  /// managers etc, and let you get stright to what you want to do: create a
  /// ModulePassManager, add passes to it, run it with the appropriate analyses.
  ///
  /// Thus, the code here is based on opt's NewPMDriver.cpp and PassBuilder.cpp
  /// Ideally this would be called SimpleAnalysisManager, but since we extend
  /// llvm::PassBuilder and also want to use it to get a default pipeline,
  /// I kept the name to prevent confusion.
  ///
  /// (8/29/2019)
  class SimplePassBuilder : public llvm::PassBuilder {
  public:
    /// @param Dbg indicates whether you want debugging enabled for the analysis passes.
    SimplePassBuilder(bool Dbg)
    : llvm::PassBuilder(nullptr, llvm::PipelineTuningOptions(), llvm::None, nullptr),
    LAM(Dbg), FAM(Dbg), CGAM(Dbg), MAM(Dbg) {}

    /// @param Dbg indicates whether you want debugging enabled for the analysis passes.
    SimplePassBuilder(llvm::TargetMachine *TM = nullptr,
                      llvm::PipelineTuningOptions PTO = llvm::PipelineTuningOptions(),
                      llvm::Optional<llvm::PGOOptions> PGOOpt = llvm::None,
                      bool Dbg = false)
    : llvm::PassBuilder(TM, PTO, PGOOpt, nullptr), LAM(Dbg), FAM(Dbg),
      CGAM(Dbg), MAM(Dbg) {}

    llvm::ModuleAnalysisManager& getAnalyses(llvm::Triple givenTriple) {
      if (!Registered)
        registerAnalyses(givenTriple);

      // make sure subsequent calls always give the same target triple.
      assert(TargetTriple.hasValue() && TargetTriple.getValue() == givenTriple);

      return MAM;
    }

  private:

    void registerAnalyses(llvm::Triple givenTriple) {
      // Register the AA manager first so that our version is the one used.
      FAM.registerPass([&] { return buildDefaultAAPipeline(); });

      // Register the target library analysis directly and give it a customized
      // preset TLI.
      TLII = std::make_unique<llvm::TargetLibraryInfoImpl>(givenTriple);
      FAM.registerPass([&] { return llvm::TargetLibraryAnalysis(*TLII); });

      // Register all the basic analyses with the managers.
      registerModuleAnalyses(MAM);
      registerCGSCCAnalyses(CGAM);
      registerFunctionAnalyses(FAM);
      registerLoopAnalyses(LAM);
      crossRegisterProxies(LAM, FAM, CGAM, MAM);

      Registered = true;
      TargetTriple = givenTriple;
    }

    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;
    std::unique_ptr<llvm::TargetLibraryInfoImpl> TLII;
    bool Registered{false};
    llvm::Optional<llvm::Triple> TargetTriple;
  };




namespace spb {
  // some independent utilities for dealing with passes, etc.

  template <typename PassT>
  inline void withPrintAfter(bool shouldPrint, llvm::ModulePassManager &MPM, PassT &&Pass) {
    auto Name = Pass.name();
    MPM.addPass(std::forward<PassT>(Pass));

    if (shouldPrint) {
      MPM.addPass(PrintIRPass(logs(), "After", Name));
    }
  }

  template <typename PassT>
  inline void withPrintAfter(bool shouldPrint, llvm::ModulePassManager &MPM, PassT *Pass) {
    auto Name = Pass->name();
    MPM.addPass(Pass);

    if (shouldPrint) {
      MPM.addPass(PrintIRPass(logs(), "After", Name));
    }
  }

  inline void addPrintPass(bool shouldPrint, llvm::ModulePassManager &MPM, llvm::StringRef Msg) {
    if (shouldPrint) {
      MPM.addPass(PrintIRPass(logs(), "At", Msg));
    }
  }

  namespace legacy {
    template <typename PassT>
    inline void withPrintAfter(bool shouldPrint, llvm::legacy::PassManager &LegacyPM, std::string Name, PassT &&Pass) {
      LegacyPM.add(std::forward<PassT>(Pass));

      if (shouldPrint) {
        Name = "\n\n;;;;;; IR Dump After " + Name + " ;;;;;;\n";
        LegacyPM.add(llvm::createPrintModulePass(logs(), Name));
      }
    }

    inline void addPrintPass(bool shouldPrint, llvm::legacy::PassManager &LegacyPM, std::string Msg) {
      if (shouldPrint) {
        Msg = "\n\n;;;;;; IR Dump At " + Msg + " ;;;;;;\n";
        LegacyPM.add(llvm::createPrintModulePass(logs(), Msg));
      }
    }
  } // end namespace legacy

}


}
