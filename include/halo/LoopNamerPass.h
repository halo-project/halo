#pragma once

#include "halo/MDUtils.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"

namespace halo {

  class LoopNamerPass : public llvm::PassInfoMixin<LoopNamerPass> {
  private:
    unsigned LoopIDs = 0;

  public:
    llvm::PreservedAnalyses run(llvm::Loop &Loop, llvm::LoopAnalysisManager&,
                          llvm::LoopStandardAnalysisResults&, llvm::LPMUpdater&) {
      llvm::MDNode *LoopMD = Loop.getLoopID();
      llvm::LLVMContext &Context = Loop.getHeader()->getContext();

      if (!LoopMD) {
        // Setup the first location with a dummy operand for now.
        llvm::MDNode *Dummy = llvm::MDNode::get(Context, {});
        LoopMD = llvm::MDNode::get(Context, {Dummy});
      }

      llvm::MDNode* Tag = createLoopName(Context, LoopIDs++);
      llvm::MDNode* Wrapper = llvm::MDNode::get(Context, {Tag});

      // combine the tag with the current LoopMD.
      LoopMD = llvm::MDNode::concatenate(LoopMD, Wrapper);

      // reinstate the self-loop in the first position of the MD.
      LoopMD->replaceOperandWith(0, LoopMD);

      Loop.setLoopID(LoopMD);

      return llvm::PreservedAnalyses::all();
    }
  };

}
