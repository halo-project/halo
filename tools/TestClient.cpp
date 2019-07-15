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

template <typename T>
void printProto(T &Value) {
  std::string AsJSON;
  proto::util::JsonPrintOptions Opts;
  Opts.add_whitespace = true;
  proto::util::MessageToJsonString(Value, &AsJSON, Opts);
  std::cout << AsJSON << "\n---\n";
}

struct Client {

  asio::io_service &IOService;
  ip::tcp::resolver Resolver;
  ip::tcp::socket Socket;
  ip::tcp::resolver::iterator EndpointIter;

  asio::streambuf OutputStreamBuffer;


  Client(asio::io_service &ioservice) :
    IOService(ioservice),
    Resolver(IOService),
    Socket(IOService) {
      EndpointIter = Resolver.resolve({ "localhost", "29000" });
    }

  void send_msg() {
    std::cout << "Sending...\n";
    halo::MessageHeader MsgHdr;
    MsgHdr.set_kind(halo::MessageKind::HELLO);

    std::ostream os(&OutputStreamBuffer);

    MsgHdr.SerializeToOstream(&os);

    // os << "hello."
    //    << "world.";

    asio::async_write(Socket, OutputStreamBuffer,
      [this](const boost::system::error_code& ec,
              std::size_t bytes_transferred) {
                  if (ec) {
                    std::cout << "there was an error!\n";
                  }
                  std::cout << "sent " << bytes_transferred << "\n";
              });
  }


  void start_session() {
    // first step is to try and connect
    asio::async_connect(Socket, EndpointIter,
          [this](boost::system::error_code Err, ip::tcp::resolver::iterator)
          {
            if (!Err) {
              std::cout << "connected!\n";
              send_msg();
            } else {
              std::cerr << "hit an error during connect phase.\n";
              IOService.stop();
            }
          });
  }

};



int main(int argc, char* argv[]) {
  cl::ParseCommandLineOptions(argc, argv, "Halo Test Client\n");

  asio::io_service IOService;
  Client client(IOService);

  client.start_session();
  IOService.run();

  return 0;

}
