#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/IRTransformLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include <memory>

namespace orc = llvm::orc;

namespace halo {

  class NoLinkingLayer : public orc::ObjectLayer {
  public:

    NoLinkingLayer(orc::ExecutionSession &ES) : orc::ObjectLayer(ES) { }

    /// Adds a MaterializationUnit representing the given IR to the given
    /// JITDylib.
    // llvm::Error add(orc::JITDylib &JD, std::unique_ptr<llvm::MemoryBuffer> O,
    //                   orc::VModuleKey K = orc::VModuleKey()) {
    //
    //                   }

    /// Emit should materialize the given IR.
    void emit(orc::MaterializationResponsibility R, std::unique_ptr<llvm::MemoryBuffer> O) override {

    };

  };

  // Based on the KaleidoscopeJIT example.
  class Compiler {
  private:
    orc::ExecutionSession ES;
    // NoLinkingLayer ObjectLayer;
    orc::RTDyldObjectLinkingLayer ObjectLayer;
    orc::IRCompileLayer CompileLayer;
    orc::IRTransformLayer OptimizeLayer;

    llvm::DataLayout DL;
    orc::MangleAndInterner Mangle;
    orc::ThreadSafeContext Ctx;

  public:
    Compiler(orc::JITTargetMachineBuilder JTMB, llvm::DataLayout DL)
        : // ObjectLayer(ES),
          ObjectLayer(ES,
                      []() { return llvm::make_unique<llvm::SectionMemoryManager>(); }),

          CompileLayer(ES, ObjectLayer, orc::ConcurrentIRCompiler(std::move(JTMB))),
          OptimizeLayer(ES, CompileLayer, optimizeModule),
          DL(std::move(DL)), Mangle(ES, this->DL),
          Ctx(llvm::make_unique<llvm::LLVMContext>()) {
      ES.getMainJITDylib().setGenerator(
          cantFail(orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
              DL.getGlobalPrefix())));
    }

    const llvm::DataLayout &getDataLayout() const { return DL; }

    llvm::LLVMContext &getContext() { return *Ctx.getContext(); }

    static llvm::Expected<std::unique_ptr<Compiler>> Create() {
      auto JTMB = orc::JITTargetMachineBuilder::detectHost();

      if (!JTMB)
        return JTMB.takeError();

      auto DL = JTMB->getDefaultDataLayoutForTarget();
      if (!DL)
        return DL.takeError();

      return llvm::make_unique<Compiler>(std::move(*JTMB), std::move(*DL));
    }

    llvm::Error addModule(std::unique_ptr<llvm::Module> M) {
      return OptimizeLayer.add(ES.getMainJITDylib(),
                               orc::ThreadSafeModule(std::move(M), Ctx));
    }

    llvm::Expected<llvm::JITEvaluatedSymbol> lookup(llvm::StringRef Name) {
      return ES.lookup({&ES.getMainJITDylib()}, Mangle(Name.str()));
    }

  private:
    static llvm::Expected<orc::ThreadSafeModule>
    optimizeModule(orc::ThreadSafeModule TSM, const orc::MaterializationResponsibility &R) {
      TSM.withModuleDo([](llvm::Module &M) {
        // Create a function pass manager.
        auto FPM = llvm::make_unique<llvm::legacy::FunctionPassManager>(&M);

        // Add some optimizations.
        FPM->add(llvm::createInstructionCombiningPass());
        FPM->add(llvm::createReassociatePass());
        FPM->add(llvm::createGVNPass());
        FPM->add(llvm::createCFGSimplificationPass());
        FPM->doInitialization();

        // Run the optimizations over all functions in the module being added to
        // the JIT.
        for (auto &F : M)
          FPM->run(F);
      });

      return std::move(TSM);
    }
  };

} // end namespace halo
