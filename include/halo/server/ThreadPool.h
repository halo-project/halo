#pragma once

#include "llvm/Support/ThreadPool.h"

#include <future>
#include <functional>
#include <utility>
#include <memory>
#include <atomic>

namespace halo {

  template<typename R>
  std::future_status get_status(std::future<R> const& Future) {
    return Future.wait_for(std::chrono::seconds(0));
  }

// A ThreadPool that supports submitting tasks that return a value.
// See asyncRet
class ThreadPool : public llvm::ThreadPool {

  // TODO: why not use std::packaged_task ??

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

  std::atomic<uint64_t> Ticket{0};

public:

  /// @returns a thread-safe way to generate a per-ThreadPool unique integer.
  uint64_t genTicket() { return Ticket.fetch_add(1);}

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
