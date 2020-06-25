#pragma once

#include "llvm/IR/PassManager.h"

namespace halo {

class LinkageFixupPass : public llvm::PassInfoMixin<LinkageFixupPass> {
private:
  llvm::StringRef RootFunc;
public:

  LinkageFixupPass(llvm::StringRef RootFuncName) : RootFunc(RootFuncName) {}

  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM) {

    for (llvm::GlobalVariable &Global : M.globals()) {
      if (Global.hasInitializer()) {
          // TODO: perhaps if it's marked constant, we can preserve the initializer
          // until after optimizations have been applied? The problem right
          // now is that ExtractGV will eliminate the initializer anyway.
          Global.setInitializer(nullptr);
      }

      // externalize this global as one that has already been initialized.
      Global.setVisibility(llvm::GlobalValue::DefaultVisibility);
      Global.setLinkage(llvm::GlobalValue::ExternalLinkage);
      Global.setDSOLocal(false);
      // Global.setExternallyInitialized(true);
    }

    for (llvm::Function &Fun : M.functions()) {
      if ( ! Fun.isDeclaration()) {
        // prepare this function definition for optimization
        Fun.removeFnAttr(llvm::Attribute::NoInline);
        Fun.removeFnAttr(llvm::Attribute::OptimizeNone);
        Fun.removeFnAttr("xray-instruction-threshold"); // we will not be patching the JIT compiled version.
        Fun.setVisibility(llvm::GlobalValue::DefaultVisibility);

        // if it's not the root function (i.e., what will be patched in)
        // then we can mark it with private linkage for calling convention
        // benefits, etc.
        if (Fun.getName() == RootFunc)
          Fun.setLinkage(llvm::GlobalValue::ExternalLinkage);
        else
          Fun.setLinkage(llvm::GlobalValue::PrivateLinkage);
      }
    }

    // force all analyses to recompute
    return llvm::PreservedAnalyses::none();
  }

};

} // end namespace
