
#include "halo/compiler/MDUtils.h"
#include "llvm/IR/Constants.h"
#include "llvm/ADT/SetVector.h"

#include <cassert>
#include <list>

using namespace llvm;

namespace halo {

  char const* TAG = "llvm.loop.id";

  Metadata* mkMDInt(IntegerType* Ty, uint64_t Val, bool isSigned) {
    auto ConstInt = ConstantInt::get(Ty, Val, isSigned);
    return ValueAsMetadata::get(ConstInt);
  }

  MDNode* createLoopName(LLVMContext& Context, unsigned LoopID) {
    MDString *Tag = MDString::get(Context, TAG);
    MDString* Val = MDString::get(Context, std::to_string(LoopID));

    MDNode *KnobTag = MDNode::get(Context, {Tag, Val});
    return KnobTag;
  }

  unsigned getLoopID(MDNode* LoopMD) {
    for (const MDOperand& Op : LoopMD->operands()) {
      MDNode *Entry = dyn_cast<MDNode>(Op.get());
      if (!Entry || Entry->getNumOperands() != 2)
        continue;

      MDString *Tag = dyn_cast<MDString>(Entry->getOperand(0).get());
      MDString *Val = dyn_cast<MDString>(Entry->getOperand(1).get());

      if (!Tag || !Val)
        continue;

      if (Tag->getString() != TAG)
        continue;

      llvm::StringRef Str = Val->getString();
      unsigned IntVal = ~0;
      Str.getAsInteger(10, IntVal);

      if (IntVal == ~0U)
        report_fatal_error("bad loop ID metadata on our tag!");

      return IntVal;
    } // end loop

    report_fatal_error("not all loops have an ID tag for tuning");
  }

  inline bool matchesLoopOption(Metadata *MD, StringRef &Key) {
      MDNode *MDN = dyn_cast<MDNode>(MD);
      if (!MDN || MDN->getNumOperands() < 1)
        return false;

      MDString *EntryKey = dyn_cast<MDString>(MDN->getOperand(0).get());

      if (!EntryKey || EntryKey->getString() != Key)
        return false;

      return true;
  }

  // a functional-style insertion with replacement that preserves all
  // non-matching operands of the MDNode, and returns a valid LoopMD.
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
  MDNode* updateLMD(MDNode *LoopMD, StringRef Key, Metadata* Val, bool DeleteOnly) {
    SmallSetVector<Metadata *, 4> MDs(LoopMD->op_begin(), LoopMD->op_end());
    LLVMContext &Cxt = LoopMD->getContext();

    // if the loop-control option already exists, remove it.
    MDs.remove_if([&](Metadata *MD) {
      return matchesLoopOption(MD, Key);
    });

    if (!DeleteOnly) {
      // add the new option.
      MDString* KeyMD = MDString::get(Cxt, Key);

      if (Val)
        MDs.insert(MDNode::get(Cxt, {KeyMD, Val}));
      else
        MDs.insert(MDNode::get(Cxt, {KeyMD}));
    } else {
      // deletion is done by key only, we do not try to match values too.
      assert(!Val && "did not expect SOME value during deletion!");
    }

    // create the new MDNode
    MDNode *NewLoopMD = MDNode::get(Cxt, MDs.getArrayRef());

    // since this is Loop metadata, we need to recreate the self-loop.
    NewLoopMD->replaceOperandWith(0, NewLoopMD);

    return NewLoopMD;
  }

} // end namespace
