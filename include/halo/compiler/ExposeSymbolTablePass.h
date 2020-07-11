//===----------------------------------------------------------------------===//
/// \file
/// This file defines a pass to change 'private' functions to have the appropriate
/// linkage visibility for Halo.
//===----------------------------------------------------------------------===//

#pragma once

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

namespace llvm {

  /// Converts 'private' functions to 'external' so that their symbols appear in the
  /// ELF object file's symbol table and we can determine the function's precise address
  /// by looking up the symbol in the DyLib.
  ///
  /// However, these changed functions should _still treated like they are private_
  /// because we are making this visibility change without care for its calling convention,
  /// which may be non-standard.
  class ExposeSymbolTablePass : public PassInfoMixin<ExposeSymbolTablePass> {
  public:
    ExposeSymbolTablePass() {}

    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
      for (auto &F : M.functions()) {
        if (F.isDeclaration())
          continue;

        if (F.hasPrivateLinkage() || F.hasInternalLinkage()) {
          F.setLinkage(GlobalValue::LinkageTypes::ExternalLinkage);
          F.setDSOLocal(true);
        }
      }
      return llvm::PreservedAnalyses::none(); // to be safe
    }
  };
} // end namespace llvm