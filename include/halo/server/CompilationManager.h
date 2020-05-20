#pragma once

#include <memory>
#include <future>
#include <list>
#include <utility>

#include "halo/compiler/CompilationPipeline.h"
#include "halo/compiler/Profiler.h"
#include "halo/tuner/KnobSet.h"
#include "halo/server/ThreadPool.h"

#include "llvm/Support/MemoryBuffer.h"

#include "Logging.h"

namespace halo {

class CompilationManager {
  public:
    using compile_expected = CompilationPipeline::compile_expected;
    using tuning_section = Profiler::TuningSection;

    struct FinishedJob {
      FinishedJob(std::string n, tuning_section ts, compile_expected res)
        : UniqueJobName(n), TS(ts), Result(std::move(res)) {}
      std::string UniqueJobName;
      tuning_section TS;
      compile_expected Result;
    };

    CompilationManager(ThreadPool &pool) : Pool(pool) {}
    void setCompilationPipeline(CompilationPipeline *cp) { Pipeline = cp; }

    void enqueueCompilation(llvm::MemoryBuffer& Bitcode,
                            tuning_section TS, KnobSet Knobs) {
      InFlight.emplace_back(genName(), TS,
          std::move(Pool.asyncRet([this,TS,&Bitcode,Knobs] () -> CompilationPipeline::compile_expected {
            std::string Name = TS.first;
            auto Result = Pipeline->run(Bitcode, Name, Knobs);

            logs() << "Finished Compile!\n";

            return Result;
        })));
    }

    llvm::Optional<FinishedJob> dequeueCompilation() {
      if (InFlight.size() == 0)
        return llvm::None;

      auto &Front = InFlight.front();
      auto &Future = Front.Promise;
      if (Future.valid() && get_status(Future) == std::future_status::ready) {
        FinishedJob Result(Front.UniqueName, Front.TS, std::move(Future.get()));
        InFlight.pop_front();
        return Result;
      }

      return llvm::None;
    }

  private:

    std::string genName() {
      auto Num = JobTicker++;
      return "<lib_" + std::to_string(Num) + ">";
    }

    struct PromisedJob {
      PromisedJob(std::string n, tuning_section ts, std::future<compile_expected> fut)
        : UniqueName(n), TS(ts), Promise(std::move(fut)) {}
      std::string UniqueName;
      tuning_section TS;
      std::future<compile_expected> Promise;
    };

    ThreadPool &Pool;
    CompilationPipeline *Pipeline;
    std::list<PromisedJob> InFlight;
    uint64_t JobTicker = 0;
};

} // end namespace