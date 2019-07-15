#include "llvm/Support/CommandLine.h"

#include "boost/asio.hpp"
#include "llvm/ADT/StringRef.h"


#include "RawSample.pb.h"
#include "MessageHeader.h"
#include "RPCSystem.h"

#include <cinttypes>
#include <iostream>
#include <list>

namespace cl = llvm::cl;
namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace hdr = halo::hdr;

namespace halo {

class ClientRPC {
private:
  asio::io_service IOService;
  ip::tcp::resolver Resolver;
  ip::tcp::socket Socket;
  ip::tcp::resolver::query Query;

public:
  halo::RPCSystem RPC;

  ClientRPC(std::string server_hostname, std::string port) :
    IOService(),
    Resolver(IOService),
    Socket(IOService),
    Query(server_hostname, port),
    RPC(Socket) { }

  // returns true if connection established.
  bool connect() {
    boost::system::error_code Err;
    ip::tcp::resolver::iterator I =
        asio::connect(Socket, Resolver.resolve(Query), Err);

    if (Err) {
      std::cerr << "Failed to connect: " << Err.message() << "\n";
      return false;
    } else {
      std::cerr << "Connected to: " << I->endpoint() << "\n";
      return true;
    }
  }

};

} // namespace halo



int main(int argc, char* argv[]) {
  cl::ParseCommandLineOptions(argc, argv, "Halo Test Client\n");

  asio::io_service IOService;
  halo::ClientRPC client("localhost", "29000");

  bool Connected = client.connect();

  if (!Connected) return 1;

  halo::RawSample RS;
  RS.set_instr_ptr(31337);

  client.RPC.send_proto(halo::hdr::RawSample, RS);

  RS.set_instr_ptr(42);
  client.RPC.send_proto(halo::hdr::RawSample, RS);

  return 0;

}
