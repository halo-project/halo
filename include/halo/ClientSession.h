#pragma once

#include "llvm/Support/ThreadPool.h"
#include "llvm/Support/TaskQueue.h"

#include "boost/asio.hpp"

// from the 'net' dir
#include "Messages.pb.h"
#include "MessageKind.h"
#include "Channel.h"

#include "halo/Profiler.h"

#include <vector>
#include <cinttypes>

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace msg = halo::msg;

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
    bool Measuring = false;
    pb::ClientEnroll Client;
    Profiler Profile;

    std::vector<pb::RawSample> RawSamples;

    ClientSession(asio::io_service &IOService, llvm::ThreadPool &TPool) :
      Socket(IOService), Chan(Socket), Queue(TPool), Status(Fresh),
      Profile(Client) {}

    void start() {
      Status = Active;
      asio::socket_base::keep_alive option(true);
      Socket.set_option(option);
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

              // The samples are likely to be noisy, so we ignore them.
              if (Measuring)
                break;

              // copy the data out into a string and save it by value in closure
              std::string Blob(Body.data(), Body.size());

              Queue.async([this,Blob](){
                RawSamples.emplace_back();
                pb::RawSample &RS = RawSamples.back();
                RS.ParseFromString(Blob);
                // msg::print_proto(RS); // DEBUG

                if (RawSamples.size() > 100)
                  Queue.async([this](){
                    Profile.analyze(RawSamples);
                    RawSamples.clear();

                    // Profile.dump(std::cerr); // DEBUG
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
                Profile.init();

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

} // end namespace halo
