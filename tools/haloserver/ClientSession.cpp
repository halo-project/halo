
#include "halo/ClientSession.h"



namespace halo {

void ClientSession::start(ClientGroup *CG) {
  Parent = CG;

  // We expect that the registrar has taken care of client enrollment.
  if (!Enrolled)
    std::cerr << "WARNING: client is not enrolled before starting!\n";

  withState([this](SessionState &State){
    // process this new enrollment
    State.Profile.init(Client);

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
          shutdown();
        } return; // NOTE: the return to ensure no more recvs are serviced.

        case msg::RawSample: {
          withState([this,Body](SessionState &State){
            enum SessionStatus Current = Status;
            if (Current == Sampling)
              std::cerr << "warning: recieved sample data while not asking for it.\n";

            // The samples are likely to be noisy, so we ignore them.
            if (Current == Measuring)
              return;

            State.RawSamples.emplace_back();
            pb::RawSample &RS = State.RawSamples.back();
            llvm::StringRef Blob(Body.data(), Body.size());
            RS.ParseFromString(Blob);
            // msg::print_proto(RS); // DEBUG

            if (State.RawSamples.size() > 25) // FIXME try to avoid hyperparameter
              withState([this](SessionState &State){
                State.Profile.analyze(State.RawSamples);
                State.RawSamples.clear();

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

llvm::Error ClientSession::translateSymbols(pb::CodeReplacement &CR) {
  // The translation here is to fill in the function addresses
  // for this client. Note that this mutates CR, but for fields we expect
  // to be overwritten.

  std::cerr << "FIX TRANSLATE SYMBOLS!\n";
  return llvm::Error::success();

  // CodeRegionInfo const& CRI = Profile.CRI;
  // proto::RepeatedPtrField<pb::FunctionSymbol> *Syms = CR.mutable_symbols();
  // for (pb::FunctionSymbol &FSym : *Syms) {
  //   auto *FI = CRI.lookup(FSym.label());
  //
  //   if (FI == nullptr)
  //     return llvm::createStringError(std::errc::not_supported,
  //         "unable to find function addr for this client.");
  //
  //   FSym.set_addr(FI->AbsAddr);
  // }
  //
  // return llvm::Error::success();
}

void ClientSession::shutdown() {
  shutdown_async();
  while (Status != Dead)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void ClientSession::shutdown_async() {
  withState([this](SessionState &State){
    Status = Dead;
  });
}

ClientSession::ClientSession(asio::io_service &IOService, ThreadPool &Pool) :
  SequentialAccess(Pool), Socket(IOService), Chan(Socket), Pool(Pool), Status(Fresh) {}



} // end namespace
