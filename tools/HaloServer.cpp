
#include "llvm/ADT/StringRef.h"
#include "llvm/BinaryFormat/MsgPackDocument.h"
#include "llvm/Support/CommandLine.h"


#include "boost/asio.hpp"

#include "Messages.pb.h"
#include "MessageKind.h"
#include "Channel.h"

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

namespace halo {

struct ClientSession {
  ip::tcp::socket Socket;
  Channel Chan;

  bool Enrolled = false;
  bool Sampling = false;
  pb::ClientEnroll Client;

  std::vector<pb::RawSample> RawSamples;

  ClientSession(asio::io_service &IOService) :
    Socket(IOService), Chan(Socket) {}

  void listen() {
    Chan.async_recv([this](msg::Kind Kind, std::vector<char>& Body) {
      std::cerr << "got msg ID " << (uint32_t) Kind << "\n";

        switch(Kind) {
          case msg::Shutdown: {
            std::cerr << "client session terminated.\n";
          } return; // NOTE: the return.

          case msg::RawSample: {
            if (!Sampling)
              std::cerr << "warning: recieved sample data while not asking for it.\n";

            RawSamples.emplace_back();
            pb::RawSample &RS = RawSamples.back();

            std::string Blob(Body.data(), Body.size());
            RS.ParseFromString(Blob);

            msg::print_proto(RS);
          } break;

          case msg::ClientEnroll: {
            if (Enrolled)
              std::cerr << "warning: recieved client enroll when already enrolled!\n";
            Enrolled = true;

            std::string Blob(Body.data(), Body.size());
            Client.ParseFromString(Blob);

            msg::print_proto(Client);

            // TEST: request sampling right away.
            Sampling = true;
            Chan.send(msg::StartSampling);

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
  ClientRegistrar(asio::io_service &service, uint32_t port)
      : Port(port),
        IOService(service),
        Endpoint(ip::tcp::v4(), Port),
        Acceptor(IOService, Endpoint) {
          accept_loop();
        }

private:
  uint32_t Port;
  asio::io_service &IOService;
  ip::tcp::endpoint Endpoint;
  ip::tcp::acceptor Acceptor;

  // TODO: periodically garbage collect dead sessions?
  std::list<ClientSession> Sessions;

  void accept_loop() {
    Sessions.emplace_back(IOService);
    auto &Session = Sessions.back();
    // NOTE: can't capture 'Session' in the lambda,
    // so currently I'm assuming we never call accept_loop
    // non-recursively more than once!
    // Garbage collection should be okay though?

    Acceptor.async_accept(Session.Socket,
      [&](boost::system::error_code Err) {
        if(!Err) {
          std::cerr << "Accepted a new connection on port " << Port << "\n";
          Sessions.back().listen();
        }
        accept_loop();
      });
  }
};

} // end namespace halo


int main(int argc, char* argv[]) {
  cl::ParseCommandLineOptions(argc, argv, "Halo Server\n");

  asio::io_service IOService;

  halo::ClientRegistrar CR(IOService, CL_Port);

  IOService.run();

  return 0;

}
