#pragma once

#include "halo/compiler/MDUtils.h"
#include "halo/tuner/NamedKnobs.h"
#include "halo/tuner/KnobSet.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"

namespace halo {

  class LoopAnnotatorPass : public llvm::PassInfoMixin<LoopAnnotatorPass> {
  private:
    KnobSet const& Knobs;

    // for two-valued flags: {!"option.name"} or deletion of that option
    // for three-valued flags: {!"option.name", i1 1}, {!"option.name", i1 0}, or deletion of that option
    llvm::MDNode* setFlagOption(llvm::MDNode *LMD, FlagKnob const& FK, llvm::StringRef OptName) {
      // if the JSON file had specified either true/false as the
      // default value for the loop knob, we assume they want a
      // "two-valued" style flag.
      if (FK.hadDefault()) {
        if (FK.isTrue()) // add the option
          return updateLMD(LMD, OptName, nullptr);
        else // delete any such options, if present
          return updateLMD(LMD, OptName, nullptr, true);
      }

      // otherwise, it's a 3 valued flag

      if (FK.isNeither()) // delete the option, if present
        return updateLMD(LMD, OptName, nullptr, true);

      llvm::LLVMContext &C = LMD->getContext();
      llvm::IntegerType* i1 = llvm::IntegerType::get(C, 1);

      assert(FlagKnob::TRUE == 1 && FlagKnob::FALSE == 0 && "assumption of code below violated.");
      assert(FK.hasVal());
      uint64_t AsInt;
      FK.applyVal([&](int RawVal) { AsInt = RawVal; });
      return updateLMD(LMD, OptName, mkMDInt(i1, AsInt));
    }

  public:
    LoopAnnotatorPass (KnobSet const& knobs) : Knobs(knobs) {}

    llvm::PreservedAnalyses run(llvm::Loop &Loop, llvm::LoopAnalysisManager&,
                          llvm::LoopStandardAnalysisResults&, llvm::LPMUpdater&) {

      llvm::MDNode* LMD = Loop.getLoopID();
        if (!LMD) {
          // LoopNamer should have been run on this module first!
          fatal_error("encountered a loop without llvm.loop metadata!");
          return llvm::PreservedAnalyses::all();
        }

        //////
        // add optimization hints to the loop

        const unsigned ID = getLoopID(LMD);
        llvm::LLVMContext &C = LMD->getContext();
        llvm::IntegerType* i32 = llvm::IntegerType::get(C, 32);

        for (std::pair<std::string, Knob::KnobKind> const& Option : named_knob::LoopOptions) {
          switch (Option.second) {
            case Knob::KK_Flag: {
              auto const& FK = Knobs.lookup<FlagKnob>(named_knob::forLoop(ID, Option));

              LMD = setFlagOption(LMD, FK, Option.first);

            } break;

            case Knob::KK_Int: {
              Knobs.lookup<IntKnob>(named_knob::forLoop(ID, Option)).applyScaledVal(
                [&](int ScaledVal) {
                  LMD = updateLMD(LMD, Option.first, mkMDInt(i32, ScaledVal));
                });

            } break;

            default:
              fatal_error("unexpected knob kind for a loop.");
          };
        }


      Loop.setLoopID(LMD);
      return llvm::PreservedAnalyses::none();
    }
  };

} // end namespace
