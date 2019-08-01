
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ThreadPool.h"
#include "llvm/Support/TaskQueue.h"


#include "boost/asio.hpp"

// from the 'net' dir
#include "Messages.pb.h"
#include "MessageKind.h"
#include "Channel.h"

#include "halo/CodeRegionInfo.h"

#include <cinttypes>
#include <iostream>
#include <list>

namespace cl = llvm::cl;
namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace msg = halo::msg;

/////////////
// Command-line Options
cl::opt<uint32_t> CL_Port("port",
                       cl::desc("TCP port to listen on."),
                       cl::init(29000));

// These options are mainly to aid in testing.

cl::opt<bool> CL_NoPersist("no-persist",
                      cl::desc("Quit once all clients have disconnected."),
                      cl::init(false));

cl::opt<uint32_t> CL_Timeout("timeout",
                      cl::desc("Quit with an error if N seconds have passed without a shutdown. Zero means disabled."),
                      cl::init(0));

namespace halo {

enum SessionStatus {
  Fresh,
  Active,
  Dead
};

struct ClientSession {
  // thread-safe members
  ip::tcp::socket Socket;
  Channel Chan;
  llvm::TaskQueue Queue;
  std::atomic<enum SessionStatus> Status;

  // all fields below here must be accessed through the sequential task queue.
  bool Enrolled = false;
  bool Sampling = false;
  pb::ClientEnroll Client;
  CodeRegionInfo CRI;

  std::vector<pb::RawSample> RawSamples;

  ClientSession(asio::io_service &IOService, llvm::ThreadPool &TPool) :
    Socket(IOService), Chan(Socket), Queue(TPool), Status(Fresh) {}

  void start() {
    Status = Active;
    listen();
  }

  void listen() {
    Chan.async_recv([this](msg::Kind Kind, std::vector<char>& Body) {
      // std::cerr << "got msg ID " << (uint32_t) Kind << "\n";

        switch(Kind) {
          case msg::Shutdown: {
            // NOTE: we enqueue this so that the job queue is flushed out.
            Queue.async([this](){
              std::cerr << "client session terminated.\n";
              Status = Dead;
            });
          } return; // NOTE: the return to ensure no more recvs are serviced.

          case msg::RawSample: {
            if (!Sampling)
              std::cerr << "warning: recieved sample data while not asking for it.\n";

            // copy the data out into a string and save it by value in closure
            std::string Blob(Body.data(), Body.size());

            Queue.async([this,Blob](){
              RawSamples.emplace_back();
              pb::RawSample &RS = RawSamples.back();
              RS.ParseFromString(Blob);
              // msg::print_proto(RS); // DEBUG

              auto Info = CRI.lookup(RS.instr_ptr());

              if (Info)
                std::cerr << "sample from " << Info.getValue()->Name << "\n";

              if (RawSamples.size() > 10)
                Queue.async([this](){
                  // TODO: process samples
                  RawSamples.clear();
                });

            });

          } break;

          case msg::ClientEnroll: {
            if (Enrolled)
              std::cerr << "warning: recieved client enroll when already enrolled!\n";

            std::string Blob(Body.data(), Body.size());

            Queue.async([this,Blob](){
              Client.ParseFromString(Blob);
              msg::print_proto(Client); // DEBUG

              // process this new enrollment
              CRI.init(Client);

              Enrolled = true;
              Sampling = true;

              // ask to sample right away for now.
              Chan.send(msg::StartSampling);
            });

          } break;

          default: {
            std::cerr << "Recieved unknown message ID: "
              << (uint32_t)Kind << "\n";
          } break;
        };

        listen();
    });
  }
};


class ClientRegistrar {
public:
  ClientRegistrar(asio::io_service &service, uint32_t port, llvm::ThreadPool &TPool)
      : Pool(TPool),
        Port(port),
        IOService(service),
        Endpoint(ip::tcp::v4(), Port),
        Acceptor(IOService, Endpoint) {
          accept_loop();
        }

  // only safe to call within the IOService. Use io_service::dispatch.
  // returns the number of sessions removed.
  void cleanup() {
    auto it = Sessions.begin();
    size_t removed = 0;
    while(it != Sessions.end()) {
      if (it->Status == Dead)
        { it = Sessions.erase(it); removed++; }
      else
        it++;
    }
    assert(removed <= ActiveSessions);
    ActiveSessions -= removed;
  }

  void consider_shutdown(bool ForcedShutdown) {
    if (ForcedShutdown ||
        (CL_NoPersist && TotalSessions > 0 && ActiveSessions == 0)) {
      IOService.stop();
    }
  }

private:
  llvm::ThreadPool &Pool;
  uint32_t Port;
  asio::io_service &IOService;
  ip::tcp::endpoint Endpoint;
  ip::tcp::acceptor Acceptor;

  // these fields must only be accessed by the IOService's thread.
  std::list<ClientSession> Sessions;
  size_t ActiveSessions = 0;
  size_t TotalSessions = 0;

  void accept_loop() {
    Sessions.emplace_back(IOService, Pool);
    auto &Session = Sessions.back();
    // NOTE: can't capture 'Session' in the lambda,
    // so currently I'm assuming we never call accept_loop
    // non-recursively more than once!
    // Garbage collection should be okay though?

    Acceptor.async_accept(Session.Socket,
      [&](boost::system::error_code Err) {
        if(!Err) {
          std::cerr << "Accepted a new connection on port " << Port << "\n";
          TotalSessions++; ActiveSessions++;
          Sessions.back().start();
        }
        accept_loop();
      });
  }
};

} // end namespace halo


int main(int argc, char* argv[]) {
  cl::ParseCommandLineOptions(argc, argv, "Halo Server\n");

  llvm::ThreadPool WorkPool;
  asio::io_service IOService;

  halo::ClientRegistrar CR(IOService, CL_Port, WorkPool);

  std::thread io_thread([&](){ IOService.run(); });

  std::cout << "Started Halo Server.\nListening on port "
            << CL_Port << std::endl;

  // loop that dispatches clean-up actions in the io_thread.
  const uint32_t SleepMS = 1000;
  const bool TimeLimited = CL_Timeout > 0;
  int64_t RemainingTime = CL_Timeout;
  bool ForceShutdown = false;

  do {
    std::this_thread::sleep_for(std::chrono::milliseconds(SleepMS));
    RemainingTime -= SleepMS;
    ForceShutdown = TimeLimited && RemainingTime <= 0;

    IOService.dispatch([&](){
      CR.cleanup();
      CR.consider_shutdown(ForceShutdown);
    });

  } while (!IOService.stopped());

  io_thread.join();

  if (ForceShutdown)
    return 1;

  return 0;
}
