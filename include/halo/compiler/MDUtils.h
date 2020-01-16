#pragma once

#include "llvm/IR/Metadata.h"
#include "llvm/IR/Function.h"
#include <list>

namespace halo {
  extern char const* TAG;

  extern char const* TRANSFORM_ATTR;

  llvm::Metadata* mkMDInt(llvm::IntegerType* Ty, uint64_t Val, bool isSigned = false);

  // return val indicates whether the module was changed
  // NOTE the transform metadata structure should follow
  // the work in Kruse's pragma branches
  bool addLoopTransformGroup(llvm::Function* F, std::list<llvm::MDNode*> &newXForms);

  llvm::MDNode* createTilingMD(llvm::LLVMContext& Cxt, const char* XFORM_NAME,
                              std::vector<std::pair<unsigned, uint16_t>> Dims);

  llvm::MDNode* createLoopName(llvm::LLVMContext& Context, unsigned LoopID);

  // parse the LoopMD, looking for the tag added by createLoopName
  unsigned getLoopName(llvm::MDNode* LoopMD);

  bool matchesLoopOption(llvm::Metadata *MD, llvm::StringRef &Key);

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
