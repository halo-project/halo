#include "llvm/Support/CommandLine.h"

#include "boost/asio.hpp"
#include "llvm/ADT/StringRef.h"


#include "RawSample.pb.h"
#include "MessageHeader.h"

#include <cinttypes>
#include <iostream>
#include <list>

namespace cl = llvm::cl;
namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace hdr = halo::hdr;

struct Client {

  asio::io_service &IOService;
  ip::tcp::resolver Resolver;
  ip::tcp::socket Socket;
  ip::tcp::resolver::iterator EndpointIter;


  Client(asio::io_service &ioservice) :
    IOService(ioservice),
    Resolver(IOService),
    Socket(IOService) {
      EndpointIter = Resolver.resolve({ "localhost", "29000" });
    }

  void send_msg() {
    static uint64_t Count = 0;
    std::cout << "Sending...\n";

    halo::RawSample RS;
    RS.set_instr_ptr(31337 + Count++);

    std::vector<asio::const_buffer> Message;

    std::string Blob;
    RS.SerializeToString(&Blob);

    hdr::MessageHeader Hdr;
    hdr::setMessageKind(Hdr, hdr::RawSample);
    hdr::setPayloadSize(Hdr, Blob.size());
    hdr::encode(Hdr);

    // queue up two items to write: the header followed by the body.
    Message.push_back(asio::buffer(&Hdr, sizeof(Hdr)));
    Message.push_back(asio::buffer(Blob));

    asio::async_write(Socket, Message,
      [this](const boost::system::error_code& ec,
              std::size_t bytes_transferred) {
                  if (ec) {
                    std::cout << "there was an error!\n";
                  }
                  std::cout << "sent " << bytes_transferred << " bytes\n";
                  if (Count < 10) send_msg();
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
