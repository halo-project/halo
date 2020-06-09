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

    // {!"option.name"}, aka, no value
    llvm::MDNode* setPresenceOption(llvm::MDNode *LMD, bool Option, llvm::StringRef OptName) {
      if (Option) // add the option
        return updateLMD(LMD, OptName, nullptr);
      else // delete any such options, if present
        return updateLMD(LMD, OptName, nullptr, true);
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

              LMD = setPresenceOption(LMD, FK.getFlag(), Option.first);

            } break;

            case Knob::KK_Int: {
              auto const& IK = Knobs.lookup<IntKnob>(named_knob::forLoop(ID, Option));

              LMD = updateLMD(LMD, Option.first, mkMDInt(i32, IK.getScaledVal()));

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
