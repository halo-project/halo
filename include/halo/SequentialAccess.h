#pragma once

#include "halo/TaskQueueOverlay.h"
#include "halo/ThreadPool.h"

namespace halo {

// provides asyncronous sequential access to state through a task queue.
template <typename StateType>
class SequentialAccess {
public:
  SequentialAccess(ThreadPool &Pool) : Queue(Pool) {}

  // ASYNC Apply the given callable to the state. Provides sequential and
  // non-overlapping access to the group's state.
  template <typename RetTy>
  std::future<RetTy> withState(std::function<RetTy(StateType&)> Callable) {
    return Queue.async([this,Callable] () {
              return Callable(State);
            });
  }

  std::future<void> withState(std::function<void(StateType&)> Callable) {
    return Queue.async([this,Callable] () {
              Callable(State);
            });
  }

private:
  // The task queue provides sequential access to the group's state.
  // The danger with locks when using a TaskPool is that if a task ever
  // blocks on a lock, that thread is stuck. There's no ability to yield / preempt
  // since there's no scheduler, so we lose threads this way.
  llvm::TaskQueueOverlay Queue;
  StateType State;
  /////////////////////////////////
}; // end class

}
