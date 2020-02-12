#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"

#include "halo/server/TaskQueueOverlay.h"
#include "halo/server/SequentialAccess.h"
#include "halo/compiler/PerformanceData.h"

#include "boost/asio.hpp"

#include "google/protobuf/repeated_field.h"

// from the 'net' dir
#include "Messages.pb.h"
#include "MessageKind.h"
#include "Channel.h"

#include <cinttypes>
#include <set>

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

  using ClientID = size_t;

  // The client's state accessed in a thread-safe manner.
  struct SessionState {
    ClientID ID; // a unique idenifier for the duration of the server process
    CodeRegionInfo CRI;
    PerformanceData PerfData;
    std::set<llvm::StringRef> DeployedCode; // TODO: merge this into CRI as a real feature
  };

  class GroupOwnedState {
  // Rather than keep this state in a seperate group-managed map, we keep
  // this state within the client session, but restrict access to it
  // from within the session. The session must synchronize with the group's
  // sequential access queue to modify its state via
  // Parent->withClientState(this, ...).
  private:
    friend class ClientGroup;
    friend class Profiler; // A profiler is owned by the group

    SessionState State;
  };

  class ClientSession : public GroupOwnedState {
  public:
    // members initialized prior to usage of this object.
    bool Enrolled = false;
    pb::ClientEnroll Client;
    size_t ID; // a unique identifier for the lifetime of a haloserver process

    // thread-safe members
    ip::tcp::socket Socket;
    Channel Chan;
    std::atomic<enum SessionStatus> Status;
    ClientGroup *Parent = nullptr;

    ClientSession(asio::io_service &IOService, ThreadPool &Pool);

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
