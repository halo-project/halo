//===----------------------------------------------------------------------===//
/// \file
/// This file defines a pass to change 'private' functions to have 'internal'
/// visibility.
//===----------------------------------------------------------------------===//

#pragma once

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

namespace llvm {

  // Converts 'private' functions to 'internal' so that their symbols appear in the
  // ELF object file's symbol table, but are otherwise treated like a private symbol.
  class ExposeSymbolTablePass : public PassInfoMixin<ExposeSymbolTablePass> {
  public:
    ExposeSymbolTablePass() {}

    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
      for (auto &F : M.functions()) {
        if (F.isDeclaration())
          continue;

        if (F.hasPrivateLinkage())
          F.setLinkage(GlobalValue::LinkageTypes::InternalLinkage);
      }

      return llvm::PreservedAnalyses::none(); // to be safe
    }
  };
} // end namespace llvm