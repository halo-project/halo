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
    using promised_compiles = std::list<std::pair<tuning_section,std::future<compile_expected>>>;

    CompilationManager(ThreadPool &pool) : Pool(pool) {}
    void setCompilationPipeline(CompilationPipeline *cp) { Pipeline = cp; }

    void enqueueCompilation(llvm::MemoryBuffer& Bitcode,
                            tuning_section TS, KnobSet Knobs) {
      InFlight.emplace_back(TS,
          std::move(Pool.asyncRet([this,TS,&Bitcode,Knobs] () -> CompilationPipeline::compile_expected {
            std::string Name = TS.first;
            auto Result = Pipeline->run(Bitcode, Name, Knobs);

            logs() << "Finished Compile!\n";

            return Result;
        })));
    }

    llvm::Optional<std::pair<tuning_section, compile_expected>> dequeueCompilation() {
      if (InFlight.size() == 0)
        return llvm::None;

      auto &Front = InFlight.front();
      auto &Future = Front.second;
      if (Future.valid() && get_status(Future) == std::future_status::ready) {
        std::pair<tuning_section, compile_expected> Result(Front.first, std::move(Future.get()));
        InFlight.pop_front();
        return Result;
      }

      return llvm::None;
    }

  private:
    ThreadPool &Pool;
    CompilationPipeline *Pipeline;
    promised_compiles InFlight;
};

} // end namespace