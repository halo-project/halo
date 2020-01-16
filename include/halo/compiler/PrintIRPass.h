#pragma once

#include "llvm/IR/PassManager.h"
#include "llvm/Support/FormatVariadic.h"

namespace halo {

// new-pass-manager compatible simple IR printer
class PrintIRPass : public llvm::PassInfoMixin<PrintIRPass> {
  llvm::SmallString<20> Banner;
  llvm::raw_ostream &Out;
public:

  /// When is typically either "Before" or "After"
  PrintIRPass(llvm::raw_ostream &out, llvm::StringRef When, llvm::StringRef PassName)
  : Out(out) {
    Banner = llvm::formatv("\n;;;;;; IR Dump {0} {1} ;;;;;;\n", When, PassName);
  }

  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM) {
    Out << Banner << M << "\n;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;\n\n";
    return llvm::PreservedAnalyses::all();
  }

};

} // end namespace
