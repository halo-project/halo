
#include "llvm/ADT/StringRef.h"
#include "llvm/BinaryFormat/MsgPackDocument.h"
#include "llvm/Support/CommandLine.h"

#include "google/protobuf/util/json_util.h"


#include "boost/asio.hpp"

#include "RawSample.pb.h"
#include "MessageKind.h"
#include "Channel.h"

#include <cinttypes>
#include <iostream>
#include <list>

namespace cl = llvm::cl;
namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace msg = halo::msg;
namespace proto = google::protobuf;

/////////////
// Command-line Options
cl::opt<uint32_t> CL_Port("port",
                       cl::desc("TCP port to listen on."),
                       cl::init(29000));

struct ClientSession {
  ip::tcp::socket Socket;
  halo::Channel Chan;
  ClientSession(asio::io_service &IOService) :
    Socket(IOService), Chan(Socket) {}

  void listen() {
    Chan.recv_proto([](msg::Kind Kind, std::vector<char>& Body) {
        switch(Kind) {
          case msg::RawSample: {
            halo::RawSample RS;
            std::string Blob(Body.data(), Body.size());
            RS.ParseFromString(Blob);

            std::string AsJSON;
            proto::util::JsonPrintOptions Opts;
            Opts.add_whitespace = true;
            proto::util::MessageToJsonString(RS, &AsJSON, Opts);
            std::cerr << AsJSON << "\n---\n";
          } break;
        };
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
      [=](boost::system::error_code Err) {
        if(!Err) {
          std::cerr << "Accepted a new connection on port " << Port << "\n";
          Sessions.back().listen();
        }
        accept_loop();
      });
  }
};


int main(int argc, char* argv[]) {
  cl::ParseCommandLineOptions(argc, argv, "Halo Server\n");

  asio::io_service IOService;

  ClientRegistrar CR(IOService, CL_Port);

  IOService.run();

  return 0;

}
