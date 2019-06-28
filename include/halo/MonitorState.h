#pragma once

#include <boost/asio.hpp>

#ifndef BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR
  #error "Boost ASIO POSIX support required"
#endif

#include <sys/signalfd.h>

#include "halo/Profiler.h"

// NOTE: we are Linux only right now, but the public interface
// will try to remain OS independent.

namespace halo {

///////////////
// Maintains the working state of the Halo Monitor thread.
// This is effectively the global state of the client-side Halo system.
class MonitorState {
private:
  int PerfFD;       // a file descriptor
  uint8_t* EventBuf;   // from mmapping the perf file descriptor.
  size_t EventBufSz;
  size_t PageSz;

  // members related to reading from perf events FD
  boost::asio::io_service PerfSignalService;
  boost::asio::posix::stream_descriptor SigSD;
  int SigFD; // TODO: do we need to close this, or will SigFD's destructor do that for us?
  signalfd_siginfo SigFDInfo;

  void handle_signalfd_read(const boost::system::error_code &Error, size_t BytesTransferred);
  void schedule_signalfd_read();

public:
  Profiler *Prof;

  MonitorState();
  ~MonitorState();

  void poll_for_perf_data();

};

} // end namespace halo
