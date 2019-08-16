#pragma once

#include "llvm/Support/ThreadPool.h"

#include <future>
#include <functional>
#include <utility>
#include <memory>

namespace halo {

// A ThreadPool that supports submitting tasks that return a value.
// See asyncRet
class ThreadPool : public llvm::ThreadPool {

  // Based on llvm/Support/TaskQueue::Task.
  // LLVM's ThreadPool only returns std::shared_future<void>.
  // Our tasks need to return values, so we need this boilerplate.
  template <typename Callable> struct Task {
    using ResultTy = typename std::result_of<Callable()>::type;

    Callable Work;
    std::shared_ptr<std::promise<ResultTy>> Promise;

    explicit Task(Callable C)
        : Work(std::move(C)), Promise(std::make_shared<std::promise<ResultTy>>()){}

    void operator()() noexcept {
      ResultTy *Dummy = nullptr;
      invokeCallbackAndSetPromise(Dummy);
    }

    template<typename T>
    void invokeCallbackAndSetPromise(T*) {
      Promise->set_value(Work());
    }

    void invokeCallbackAndSetPromise(void*) {
      Work();
      Promise->set_value();
    }
  };

public:

  template <typename Callable>
  inline std::future<typename std::result_of<Callable()>::type> asyncRet(Callable &&Fun) {
    Task<Callable> Wrapper(Fun);

    using ResultTy = typename std::result_of<Callable()>::type;
    std::future<ResultTy> Future = Wrapper.Promise->get_future();

    async(std::move(Wrapper));

    return Future;
  }

};

}
