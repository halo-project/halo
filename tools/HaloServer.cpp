
#include "llvm/ADT/StringRef.h"
#include "llvm/BinaryFormat/MsgPackDocument.h"
#include "llvm/Support/CommandLine.h"

#include "google/protobuf/util/json_util.h"


#include "boost/asio.hpp"

#include "RawSample.pb.h"
#include "MessageHeader.h"

#include <cinttypes>
#include <iostream>
#include <list>

namespace cl = llvm::cl;
namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace hdr = halo::hdr;
namespace proto = google::protobuf;

/////////////
// Command-line Options
cl::opt<uint32_t> CL_Port("port",
                       cl::desc("TCP port to listen on."),
                       cl::init(29000));

std::vector<uint8_t> make_vector(asio::streambuf& streambuf) {
  return {asio::buffers_begin(streambuf.data()),
          asio::buffers_end(streambuf.data())};
}

class ClientSession {
public:
  ClientSession(std::unique_ptr<ip::tcp::socket> socket)
      : Socket(std::move(socket)), Stopped(false) {
      }

  void start() { listen(); }
  void end() { Stopped = true; }
  bool has_stopped() const { return Stopped; }

  void listen() {
    // read the header
    boost::asio::async_read(*Socket, asio::buffer(&Hdr, sizeof(Hdr)),
      [this](boost::system::error_code Err, size_t Size) {
        if (Err) {
          std::cerr << "status: " << Err.message() << "\n";
          end(); return;
        }

        hdr::decode(Hdr);
        hdr::MessageKind Kind = hdr::getMessageKind(Hdr);
        uint32_t PayloadSz = hdr::getPayloadSize(Hdr);

        // perform another read for the body of specified length.
        Body.resize(PayloadSz);
        boost::asio::async_read(*Socket, asio::buffer(Body),
          [this](boost::system::error_code Err, size_t Size) {
            if (Err) {
              std::cerr << "status @ body: " << Err.message() << "\n";
              end(); return;
            }

            halo::RawSample RS;
            std::string Blob(Body.data(), Body.size());
            RS.ParseFromString(Blob);

            std::string AsJSON;
            proto::util::JsonPrintOptions Opts;
            Opts.add_whitespace = true;
            proto::util::MessageToJsonString(RS, &AsJSON, Opts);
            std::cerr << AsJSON << "\n---\n";

            /*
            llvm::StringRef Blob(Body.data(), Body.size());

            msgpack::Document Doc;
            bool Success = Doc.readFromBlob(Blob, false);

            if (Success) {
              Doc.toYAML(llvm::errs());
            } else {
              std::cerr << "Error parsing blob.\n";
            }
            */

            // go back to starting state.
            listen();
          });
      });
  }

private:
  std::unique_ptr<ip::tcp::socket> Socket;
  bool Stopped;
  hdr::MessageHeader Hdr;
  std::vector<char> Body;
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
          std::cerr << "Accepted a new connection on port " << Port << "\n";
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
