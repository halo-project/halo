#pragma once

#include "llvm/IR/Metadata.h"
#include "llvm/IR/Function.h"
#include <list>

namespace halo {
  extern char const* TAG;

  llvm::Metadata* mkMDInt(llvm::IntegerType* Ty, uint64_t Val, bool isSigned = false);

  // given the loop ID, generates an LLVM IR metadata node that is equivalent to it.
  llvm::MDNode* createLoopName(llvm::LLVMContext& Context, unsigned LoopID);

  // parse the LoopMD, looking for the tag added by createLoopName
  unsigned getLoopID(llvm::MDNode* LoopMD);

  // a functional-style insertion with replacement that preserves all
  // non-matching operands of the llvm::MDNode, and returns a valid LoopMD.
  // This function can also be used to delete an entry of the given key if nullptr is provided as the Val.
  //
  // example:
  //
  //  BEFORE:
  //
  // !x = {!x, ... !1, ...}
  // !1 = {"loop.vectorize.enable", i32 1}
  //
  //  AFTER THE FOLLOWING ACTION
  //
  // updateLMD(!x, "loop.vectorize.enable", i32 0)
  //
  // !y = {!y, ... !1, ...}
  // !1 = {"loop.vectorize.enable", i32 0}
  //
  // The node !y will be returned.
  //
  llvm::MDNode* updateLMD(llvm::MDNode *LoopMD, llvm::StringRef Key, llvm::Metadata* Val, bool DeleteOnly = false);

} // end namespace
