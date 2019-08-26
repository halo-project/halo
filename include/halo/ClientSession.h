#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"
#include "halo/TaskQueueOverlay.h"
#include "halo/SequentialAccess.h"

#include "boost/asio.hpp"

#include "google/protobuf/repeated_field.h"

// from the 'net' dir
#include "Messages.pb.h"
#include "MessageKind.h"
#include "Channel.h"

#include "halo/Profiler.h"

#include <vector>
#include <cinttypes>

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace msg = halo::msg;
namespace proto = google::protobuf;

namespace halo {

  class ClientGroup;

  enum SessionStatus {
    Fresh,
    Active,
    Sampling,
    Measuring,
    Dead
  };

  struct SessionState {
    Profiler Profile;
    std::vector<pb::RawSample> RawSamples;
  };

  class ClientSession : public SequentialAccess<SessionState> {
  public:
    // members initialized prior to usage of this object.
    bool Enrolled = false;
    pb::ClientEnroll Client;

    // thread-safe members
    ip::tcp::socket Socket;
    Channel Chan;
    std::atomic<enum SessionStatus> Status;
    ClientGroup *Parent = nullptr;

    ClientSession(asio::io_service &IOService, ThreadPool &Pool);

    // Mutates the input CodeReplacement message, replacing the absolute addresses
    // of the function symbols contained, such that they match this client session.
    llvm::Error translateSymbols(pb::CodeReplacement &CR);

    // a blocking version of shutdown_async
    void shutdown();

    // shuts down this client by flushing all
    // jobs in its queue and TODO closes the socket. The status is then
    // set to Dead.
    void shutdown_async();

    // finishes initialization of the client and
    // kicks off the async interaction loop for this client session in the IOService.
    void start(ClientGroup *CG);

private:
    void listen();

  };

} // end namespace halo
