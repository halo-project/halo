#pragma once

#include "halo/tuner/TuningSection.h"

namespace halo {
  class CompileOnceTuningSection : public TuningSection {
  public:

    CompileOnceTuningSection(TuningSectionInitializer TSI, std::string RootFunc) : TuningSection(TSI, RootFunc) {
      // TODO: this would be better if it were just a knob set with one knob in it, and the pipeline
      // skips settings for which there is a missing knob!
      KnobSet Config(BaseKnobs);

      auto FixedOptLevel = llvm::PassBuilder::OptimizationLevel::O3;
      auto &OK = Config.lookup<OptLvlKnob>(named_knob::OptimizeLevel);

      assert(OK.getMin() <= FixedOptLevel && FixedOptLevel <= OK.getMax());
      OK.setVal(FixedOptLevel);

      Compiler.enqueueCompilation(*Bitcode, std::move(Config));
      Status = ActivityState::WaitingForCompile;
    }

    void take_step(GroupState &State) override {
      if (Status == ActivityState::WaitingForCompile) {
        auto Finished = Compiler.dequeueCompilation();
        if (!Finished)
          return;

        CodeVersion NewCV {std::move(Finished.getValue())};
        LibName = NewCV.getLibraryName();
        Versions[LibName] = std::move(NewCV);
        Status = ActivityState::Deployed;
      }

      assert(Status == ActivityState::Deployed);

      sendLib(State, Versions[LibName]);
      redirectTo(State, Versions[LibName]);
    }

    void dump() const override {
      const std::string StatName = (Status == ActivityState::WaitingForCompile
                                  ? "WaitingForCompile"
                                  : "Deployed");

      clogs() << "CompileOnceTuningSection {"
              << "\n\tStatus = " << StatName
              << "\n}\n";
    }

  private:

    enum class ActivityState {
      WaitingForCompile,
      Deployed
    };

    ActivityState Status;
    std::string LibName;
  };
} // end namespace