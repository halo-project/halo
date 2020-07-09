#pragma once

#include "halo/tuner/TuningSection.h"
#include "halo/tuner/ConfigManager.h"

namespace halo {
  class CompileOnceTuningSection : public TuningSection {
  public:

    CompileOnceTuningSection(TuningSectionInitializer TSI, std::string RootFunc) : TuningSection(TSI, RootFunc) {
      auto MaybeConfig = Manager.genExpertOpinion(BaseKnobs);

      if (!MaybeConfig)
        fatal_error("jitonce strategy failed: config manager has no expert opinion?");

      Compiler.enqueueCompilation(*Bitcode, std::move(MaybeConfig.getValue()));
      Status = ActivityState::WaitingForCompile;
    }

    void take_step(GroupState &State) override {
      // consume & decay so we don't leak memory.
      Profile.consumePerfData(State);
      Profile.decay();

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
    ConfigManager Manager;
  };
} // end namespace