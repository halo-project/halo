#pragma once

#include "llvm/IR/PassManager.h"

namespace halo {

class LinkageFixupPass : public llvm::PassInfoMixin<LinkageFixupPass> {
public:

  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM) {

    for (llvm::GlobalVariable &Global : M.globals()) {
      if (Global.hasInitializer() && !Global.isConstant()) {
          // NOTE: perhaps if it's not marked constant, it could be deduced
          // through analysis? For example, internal and definitely only read
          // from.
          Global.setInitializer(nullptr);
      }

      // externalize this global
      Global.setVisibility(llvm::GlobalValue::DefaultVisibility);
      Global.setLinkage(llvm::GlobalValue::ExternalLinkage);
    }

    for (llvm::Function &Fun : M.functions()) {
      if ( ! Fun.isDeclaration()) {
        // internalize this function and prepare it for optimization
        Fun.removeFnAttr(llvm::Attribute::NoInline);
        Fun.removeFnAttr(llvm::Attribute::OptimizeNone);
        Fun.setVisibility(llvm::GlobalValue::DefaultVisibility);
        Fun.setLinkage(llvm::GlobalValue::PrivateLinkage);
      }
    }

    // TODO: llvm.ctors and aliases etc?

    // force all analyses to recompute
    return llvm::PreservedAnalyses::none();
  }

};

} // end namespace
