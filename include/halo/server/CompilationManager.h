#pragma once

#include <memory>
#include <future>
#include <list>
#include <utility>
#include <chrono>

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
      FinishedJob(std::string n, KnobSet &&c, compile_expected res)
        : UniqueJobName(n), Config(std::move(c)), Result(std::move(res)) {}
      std::string UniqueJobName;
      KnobSet Config;
      compile_expected Result;
    };

    CompilationManager(ThreadPool &pool, CompilationPipeline &pipeline) : Pool(pool), Pipeline(pipeline)  {}

    void enqueueCompilation(llvm::MemoryBuffer& Bitcode, KnobSet Knobs) {
      InFlight.emplace_back(genName(), Knobs,
          std::move(Pool.asyncRet([this,&Bitcode,Knobs] () -> CompilationPipeline::compile_expected {

            // We want to compile jobs to have low priority. Two reasons for this:
            // (1) We want the other thread pool that manages everything else to remain reponsive.
            // (2) Just in case the server is being run on the same machine as the client,
            //     we don't want to bog down the client with long-running compile jobs.

            llvm::set_thread_priority(llvm::ThreadPriority::Background);

            auto Start = std::chrono::system_clock::now();

            auto Result = Pipeline.run(Bitcode, Knobs);

            auto End = std::chrono::system_clock::now();
            std::chrono::duration<float> Diff = End - Start;
            clogs(LC_Compiler) << "Compile job finished in " << Diff.count() << " seconds.\n";

            return Result;
        })));
    }

    size_t jobsInFlight() const { return InFlight.size(); }

    // Dequeues and returns any finished job, if one is available.
    llvm::Optional<FinishedJob> dequeueCompilation() {
      if (InFlight.size() == 0)
        return llvm::None;

      for (auto I = InFlight.begin(); I != InFlight.end(); ++I) {
        auto &Future = I->Promise;
        if (Future.valid() && get_status(Future) == std::future_status::ready) {
          FinishedJob Result(I->UniqueName, std::move(I->Config), std::move(Future.get()));
          InFlight.erase(I);
          return Result;
        }
      }

      return llvm::None;
    }

  private:

    std::string genName() {
      auto Num = Pool.genTicket();
      return "#lib_" + std::to_string(Num) + "#";
    }

    struct PromisedJob {
      PromisedJob(std::string n, KnobSet c, std::future<compile_expected> fut)
        : UniqueName(n), Config(c), Promise(std::move(fut)) {}
      std::string UniqueName;
      KnobSet Config;
      std::future<compile_expected> Promise;
    };

    ThreadPool &Pool;
    CompilationPipeline &Pipeline;
    std::list<PromisedJob> InFlight;
};

} // end namespace