#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ThreadPool.h"
#include "llvm/Support/Error.h"
#include "halo/TaskQueueOverlay.h"
#include "halo/ClientGroup.h"

#include "boost/asio.hpp"

#include "google/protobuf/repeated_field.h"

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
namespace proto = google::protobuf;

namespace halo {

  class ClientGroup;

  enum SessionStatus {
    Fresh,
    Active,
    Dead
  };

  struct ClientSession {
    // members initialized prior to usage of this object.
    bool Enrolled = false;
    pb::ClientEnroll Client;

    // thread-safe members
    ip::tcp::socket Socket;
    Channel Chan;
    llvm::ThreadPool &Pool;
    llvm::TaskQueueOverlay Queue;
    std::atomic<enum SessionStatus> Status;
    ClientGroup *Parent = nullptr;

    // all fields below here must be accessed through the sequential task queue.
    bool Sampling = false;
    bool Measuring = false;

    Profiler Profile;

    std::vector<pb::RawSample> RawSamples;

    ClientSession(asio::io_service &IOService, llvm::ThreadPool &Pool) :
      Socket(IOService), Chan(Socket), Pool(Pool), Queue(Pool), Status(Fresh),
      Profile(Client) {}

    // Mutates the input CodeReplacement message, replacing the absolute addresses
    // of the function symbols contained, such that they match this client session.
    llvm::Error translateSymbols(pb::CodeReplacement &CR) {
      // The translation here is to fill in the function addresses
      // for this client. Note that this mutates CR, but for fields we expect
      // to be overwritten.
      CodeRegionInfo const& CRI = Profile.CRI;
      proto::RepeatedPtrField<pb::FunctionSymbol> *Syms = CR.mutable_symbols();
      for (pb::FunctionSymbol &FSym : *Syms) {
        auto *FI = CRI.lookup(FSym.label());

        if (FI == nullptr)
          return llvm::createStringError(std::errc::not_supported,
              "unable to find function addr for this client.");

        FSym.set_addr(FI->AbsAddr);
      }

      return llvm::Error::success();
    }

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
        Sampling = true;
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


              Queue.async([this,Body](){
                RawSamples.emplace_back();
                pb::RawSample &RS = RawSamples.back();
                llvm::StringRef Blob(Body.data(), Body.size());
                RS.ParseFromString(Blob);
                // msg::print_proto(RS); // DEBUG

                if (RawSamples.size() > 25) // FIXME try to avoid hyperparameter
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
