
#include "halo/ClientSession.h"
#include "halo/ClientGroup.h"


namespace halo {

void ClientSession::start(ClientGroup *CG) {
  Parent = CG;

  // We expect that the registrar has taken care of client enrollment.
  if (!Enrolled)
    std::cerr << "WARNING: client is not enrolled before starting!\n";

  Parent->withClientState(this, [this](SessionState &State){
    // process this new enrollment
    State.Data.init(Client);

    // ask to sample right away for now.
    Chan.send(msg::StartSampling);
    Status = Sampling;
  });

  listen();
}

void ClientSession::listen()  {
  Chan.async_recv([this](msg::Kind Kind, std::vector<char>& Body) {
    // std::cerr << "got msg ID " << (uint32_t) Kind << "\n";

      switch(Kind) {
        case msg::Shutdown: {
          shutdown_async();
        } return; // NOTE: the return to ensure no more recvs are serviced.

        case msg::RawSample: {

          Parent->withClientState(this, [this,Body](SessionState &State) {
            pb::RawSample RS;
            llvm::StringRef Blob(Body.data(), Body.size());
            RS.ParseFromString(Blob);
            State.Data.add(RS);
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

void ClientSession::shutdown() {
  shutdown_async();
  while (Status != Dead)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void ClientSession::shutdown_async() {
  Parent->withClientState(this, [this](SessionState &State){
    Status = Dead;
  });
}

ClientSession::ClientSession(asio::io_service &IOService, ThreadPool &Pool) :
  Socket(IOService), Chan(Socket), Status(Fresh) {}



} // end namespace
