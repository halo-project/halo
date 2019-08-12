#pragma once

#include "llvm/Support/ThreadPool.h"
#include "llvm/Support/TaskQueue.h"
#include "halo/ClientGroup.h"

#include "boost/asio.hpp"

// from the 'net' dir
#include "Messages.pb.h"
#include "MessageKind.h"
#include "Channel.h"

#include "halo/Profiler.h"
#include "halo/Compiler.h"

#include <vector>
#include <cinttypes>

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace msg = halo::msg;

namespace halo {

  class ClientGroup;

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
    ClientGroup *Parent = nullptr;

    // all fields below here must be accessed through the sequential task queue.
    bool Enrolled = false;
    bool Sampling = false;
    bool Measuring = false;
    pb::ClientEnroll Client;
    Profiler Profile;
    // Compiler Compile;

    std::vector<pb::RawSample> RawSamples;

    ClientSession(asio::io_service &IOService, llvm::ThreadPool &TPool) :
      Socket(IOService), Chan(Socket), Queue(TPool), Status(Fresh),
      Profile(Client) {}

    void shutdown() {
      // NOTE: we enqueue this so that the job queue is flushed out.
      Queue.async([this](){
        std::cerr << "client session terminated.\n";
        Status = Dead;
      });
    }

    void start(ClientGroup *CG) {
      Parent = CG;

      // We expect that the registrar has taken care of client enrollment.
      if (!Enrolled)
        std::cerr << "WARNING: client is not enrolled before starting!\n";

      Queue.async([this](){
        // process this new enrollment
        Profile.init();

        // ask to sample right away for now.
        Chan.send(msg::StartSampling);
      });

      listen();
    }

private:
    void listen() {
      Chan.async_recv([this](msg::Kind Kind, std::vector<char>& Body) {
        // std::cerr << "got msg ID " << (uint32_t) Kind << "\n";

          switch(Kind) {
            case msg::Shutdown: {
              shutdown();
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
              std::cerr << "warning: recieved client enroll when already enrolled!\n";
            } break;

            default: {
              std::cerr << "Recieved unknown message ID: "
                << (uint32_t)Kind << "\n";
            } break;
          };

          listen();
      });
    } // end listen

  };

} // end namespace halo
