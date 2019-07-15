#pragma once

#include "MessageHeader.h"

#include "boost/asio.hpp"

#include <iostream>
#include <functional>

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace hdr = halo::hdr;

namespace halo {

  // This class contains common code for RPC-style duplex communication
  // over a socket.
  class RPCSystem {
  public:
      RPCSystem(ip::tcp::socket &Socket) : Sock(Socket) {}

      // synchronous send operation for a given proto buffer.
      template<typename T>
      void send_proto(hdr::MessageKind Kind, T& ProtoBuf) {
        std::vector<asio::const_buffer> Msg;

        std::string Blob;
        ProtoBuf.SerializeToString(&Blob);

        hdr::MessageHeader Hdr;
        hdr::setMessageKind(Hdr, Kind);
        hdr::setPayloadSize(Hdr, Blob.size());
        hdr::encode(Hdr);

        // queue up two items to write: the header followed by the body.
        Msg.push_back(asio::buffer(&Hdr, sizeof(Hdr)));
        Msg.push_back(asio::buffer(Blob));

        boost::system::error_code Err;
        // size_t BytesWritten =
            asio::write(Sock, Msg, Err);

        if (Err) {
          std::cerr << "send error: " << Err.message() << "\n";
        }
      }

      void recv_proto(std::function<void(hdr::MessageKind, std::vector<char>&)> Callback) {
        // since it's async, we need to worry about lifetimes. could be
        // multiple enqueued reads.
        hdr::MessageHeader *HdrBuf = new hdr::MessageHeader();

        // read the header
        asio::async_read(Sock, asio::buffer(HdrBuf, sizeof(hdr::MessageHeader)),
          [=](boost::system::error_code Err1, size_t Size) {
            if (Err1) {
              std::cerr << "status: " << Err1.message() << "\n";
              return;
            }

            hdr::MessageHeader Hdr = *HdrBuf;
            delete HdrBuf;

            hdr::decode(Hdr);
            hdr::MessageKind Kind = hdr::getMessageKind(Hdr);
            uint32_t PayloadSz = hdr::getPayloadSize(Hdr);

            std::vector<char> Body;

            // perform another read for the body of specified length.
            // This one is synchronous since the body should already be here.
            Body.resize(PayloadSz);
            boost::system::error_code Err2;
            // size_t BytesRead =
              boost::asio::read(Sock, asio::buffer(Body), Err2);

            if (Err2) {
              std::cerr << "status @ body: " << Err2.message() << "\n";
              return;
            }

            Callback(Kind, Body);
          });
      }

  private:
    ip::tcp::socket &Sock;

  };


}
