#include <thread>
#include <atomic>
#include <iostream>

#include "halo/MonitorState.h"

namespace halo {

////////////////////////////////////////////////
// Main loop of the Halo Monitor
void monitor_loop(MonitorState &M, std::atomic<bool> &ShutdownRequested) {
  /////////
  // Setup

  while (!ShutdownRequested) {

    M.poll_for_perf_data();

    // TODO: If there is new profile data, process it.

    // TODO: communicate with optimization server and perform code replacement
    // experiments as needed.


    std::this_thread::sleep_for(std::chrono::milliseconds(50));

  } // end of event loop
}


class HaloMonitor {
private:
  std::thread MonitorThread;
  std::atomic<bool> ShutdownRequested;
  MonitorState State;
public:
  /////////////////////////////////////////////////////////////////////////
  // The main entry-point to start halo's process monitoring system.
  HaloMonitor() : ShutdownRequested(false) {
    // start the monitor thread
    MonitorThread = std::thread(monitor_loop,
                                  std::ref(State), std::ref(ShutdownRequested));

    std::cerr << "Halo Running!\n";
  }

  /////////////////////////////////////////////////////////////////////////
  // shut down the monitor
  ~HaloMonitor() {
    // stop the monitor thread gracefully
    ShutdownRequested = true;
    MonitorThread.join();
  }
};


static HaloMonitor SystemMonitor;

///////////////////
// Static initalizer which causes programs this library is linked with
// to start the monitor.
// Caveats:
//
//     (1) statically linked libraries. In this case, you need to create a
//         .so file that is a linker script to force the linking of this object
//         file. Otherwise, you'll need to create some sort of dependency on
//         this file to ensure it is not dropped!
//
//     (2) the order of static initalizers is technically undefined,
//         so other static initializers may launch threads that we
//         miss out on profiling!
//
// Other ideas:
//  - ld main.o --undefined=__my_static_ctor -lhalomon
//         this injects a dependency on a symbol, to force halo lib to be linked.
//
//  - -Wl,-no-as-needed halolib.so -Wl,-as-needed
//
//  - make the .so file a linker script that demands halomon be included.
///////////////////

} // end namespace halo
