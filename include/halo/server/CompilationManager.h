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

    struct FinishedJob {
      FinishedJob(std::string n, compile_expected res)
        : UniqueJobName(n), Result(std::move(res)) {}
      std::string UniqueJobName;
      compile_expected Result;
    };

    CompilationManager(ThreadPool &pool, CompilationPipeline &pipeline) : Pool(pool), Pipeline(pipeline)  {}

    void enqueueCompilation(llvm::MemoryBuffer& Bitcode, KnobSet Knobs) {
      InFlight.emplace_back(genName(),
          std::move(Pool.asyncRet([this,&Bitcode,Knobs] () -> CompilationPipeline::compile_expected {
            return Pipeline.run(Bitcode, Knobs);
        })));
    }

    llvm::Optional<FinishedJob> dequeueCompilation() {
      if (InFlight.size() == 0)
        return llvm::None;

      auto &Front = InFlight.front();
      auto &Future = Front.Promise;
      if (Future.valid() && get_status(Future) == std::future_status::ready) {
        FinishedJob Result(Front.UniqueName, std::move(Future.get()));
        InFlight.pop_front();
        return Result;
      }

      return llvm::None;
    }

  private:

    std::string genName() {
      auto Num = Pool.genTicket();
      return "#lib_" + std::to_string(Num) + "#";
    }

    struct PromisedJob {
      PromisedJob(std::string n, std::future<compile_expected> fut)
        : UniqueName(n), Promise(std::move(fut)) {}
      std::string UniqueName;
      std::future<compile_expected> Promise;
    };

    ThreadPool &Pool;
    CompilationPipeline &Pipeline;
    std::list<PromisedJob> InFlight;
};

} // end namespace