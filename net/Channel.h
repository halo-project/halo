#pragma once

#include "MessageHeader.h"
#include "Logging.h"

#include "boost/asio.hpp"

#include <iostream>
#include <functional>

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace msg = halo::msg;

namespace halo {

  // This class contains common code for implementing a duplex message channel
  // over a common TCP socket. The 'channel' here loosly based on concepts from
  // Concurrent ML.
  class Channel {
  public:
      Channel(ip::tcp::socket &Socket) : Sock(Socket) {}

      /// synchronous send operation for a given proto buffer.
      /// @returns true if there was an error.
      template<typename T>
      bool send_proto(msg::Kind Kind, T& ProtoBuf) {
        if(SeenError || !Sock.is_open())
          return true;

        std::vector<asio::const_buffer> Msg;

        std::string Blob;
        ProtoBuf.SerializeToString(&Blob);

        msg::Header Hdr;
        msg::setMessageKind(Hdr, Kind);
        msg::setPayloadSize(Hdr, Blob.size());
        msg::encode(Hdr);

        // queue up two items to write: the header followed by the body.
        Msg.push_back(asio::buffer(&Hdr, sizeof(Hdr)));
        Msg.push_back(asio::buffer(Blob));

        boost::system::error_code Err;
      #ifndef NDEBUG
        size_t BytesWritten =
      #endif
            asio::write(Sock, Msg, Err);

        if (Err) {
          logs(LC_Channel) << "socket event (send_proto): " << Err.message() << "\n";
          SeenError = true;
          return true;
        }

        assert(BytesWritten > 0 && "no data written?"); // TODO: this could be strenghened if we calculated the expected message size
        return false;
      }


      /// synchronous send operation of a message with no payload
      /// @returns true if there was an error.
      bool send(msg::Kind Kind) {
        if(SeenError || !Sock.is_open())
          return true;

        msg::Header Hdr;
        msg::setMessageKind(Hdr, Kind);
        msg::setPayloadSize(Hdr, 0);
        msg::encode(Hdr);

        boost::system::error_code Err;
      #ifndef NDEBUG
        size_t BytesWritten =
      #endif
            asio::write(Sock, asio::buffer(&Hdr, sizeof(Hdr)), Err);

        if (Err) {
          logs(LC_Channel) << "socket event (send): " << Err.message() << "\n";
          SeenError = true;
          return true;
        }

        assert(BytesWritten > 0 && "no data written?");
        return false;
      }


      /// synchronously recieve an arbitrary message
      /// @returns true if there was an error
      bool recv(std::function<void(msg::Kind, std::vector<char>&)> Callback) {
        if(SeenError || !Sock.is_open())
          return recv_error(Callback, "socket is closed");

        msg::Header Hdr;
        boost::system::error_code Err1;

      #ifndef NDEBUG
        size_t BytesRead =
      #endif
          asio::read(Sock, asio::buffer(&Hdr, sizeof(msg::Header)), Err1);

        if (Err1)
          return recv_error(Callback, Err1.message());

        assert(BytesRead == sizeof(msg::Header) && "incomplete message header read");
        return recv_body(Hdr, Callback);
      }


      /// asynchronously recieve an arbitrary message. To poll / query, you
      /// need to run the IOService associated with this Socket.
      void async_recv(std::function<void(msg::Kind, std::vector<char>&)> Callback) {
        // since it's async, we need to worry about lifetimes. could be
        // multiple enqueued reads.
        msg::Header *HdrBuf = new msg::Header();

        // read the header
        asio::async_read(Sock, asio::buffer(HdrBuf, sizeof(msg::Header)),
          [=](boost::system::error_code Err1, size_t Size) {
            msg::Header Hdr = *HdrBuf;
            delete HdrBuf;

            if (Err1) {
              recv_error(Callback, Err1.message());
              return;
            }

            recv_body(Hdr, Callback);
          });
      }

      // returns true if the socket has bytes available for reading.
      bool has_data() {
        return !SeenError && Sock.is_open() && Sock.available() > 0;
      }

  private:
    ip::tcp::socket &Sock;
    bool SeenError{false};

    bool recv_body(msg::Header Hdr, std::function<void(msg::Kind, std::vector<char>&)> Callback) {
      msg::decode(Hdr);
      msg::Kind Kind = msg::getMessageKind(Hdr);
      uint32_t PayloadSz = msg::getPayloadSize(Hdr);

      std::vector<char> Body;

      // perform another read for the body of specified length.
      // This one is synchronous since the body should already be here.
      Body.resize(PayloadSz);
      boost::system::error_code Err2;
    #ifndef NDEBUG
      size_t BytesRead =
    #endif
        asio::read(Sock, asio::buffer(Body), Err2);

      if (Err2)
        return recv_error(Callback, Err2);

      assert(BytesRead == PayloadSz && "incomplete read of message body");
      Callback(Kind, Body);
      return false; // no error
    }

    template<typename T>
    bool recv_error(const std::function<void(msg::Kind, std::vector<char>&)> &Callback,
                    T ErrMsg) {
      clogs(LC_Channel) << "socket event (recv): " << ErrMsg << "\n";
      SeenError = true;
      std::vector<char> Empty;
      Callback(msg::Shutdown, Empty);
      return true; // yes error
    }

  };


}
