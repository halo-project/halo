#pragma once

#include "llvm/IR/PassManager.h"

namespace halo {

class ExternalizeGlobalsPass : public llvm::PassInfoMixin<ExternalizeGlobalsPass> {
public:

  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM) {

    for (llvm::GlobalVariable &Global : M.globals()) {
      llvm::errs() << Global;
    }

    return llvm::PreservedAnalyses::all();
  }

};

} // end namespace
