#include "llvm/Support/CommandLine.h"

#include "boost/asio.hpp"
#include "google/protobuf/util/json_util.h"

#include "Comms.pb.h"

#include <cinttypes>
#include <iostream>
#include <list>

namespace cl = llvm::cl;
namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace proto = google::protobuf;

/////////////
// Command-line Options
cl::opt<uint32_t> CL_Port("port",
                       cl::desc("TCP port to listen on."),
                       cl::init(29000));

template <typename T>
void printProto(T &Value) {
  std::string AsJSON;
  proto::util::JsonPrintOptions Opts;
  Opts.add_whitespace = true;
  proto::util::MessageToJsonString(Value, &AsJSON, Opts);
  std::cout << AsJSON << "\n---\n";
}

class ClientSession {
public:
  ClientSession(std::unique_ptr<ip::tcp::socket> socket)
      : Socket(std::move(socket)) {
      }

  void start() {  listen(); }

  void listen() {
    boost::asio::async_read(*Socket, InputStreamBuffer,
      [this](boost::system::error_code Err, size_t Size) {
        std::istream is(&InputStreamBuffer);
        MsgHdr.ParseFromIstream(&is);
        printProto(MsgHdr);

        listen();
      });
  }

private:
  std::unique_ptr<ip::tcp::socket> Socket;
  asio::streambuf InputStreamBuffer;
  halo::MessageHeader MsgHdr;
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
  std::unique_ptr<ip::tcp::socket> FreshSocket;

  // TODO: periodically garbage collect dead sessions?
  std::list<ClientSession> Sessions;

  void accept_loop() {
    FreshSocket = std::make_unique<ip::tcp::socket>(IOService);

    Acceptor.async_accept(*FreshSocket,
      [this](boost::system::error_code Err) {
        if(!Err) {
          std::cout << "Accepted a new connection on port " << Port << "\n";
          Sessions.emplace_back(std::move(FreshSocket));
          Sessions.back().start();
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
