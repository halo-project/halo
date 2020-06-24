
#include "halo/server/ClientSession.h"
#include "halo/server/ClientGroup.h"


namespace halo {

void ClientSession::send_library(SessionState &MyState, pb::LoadDyLib const& DylibMsg) {
  auto &DeployedLibs = MyState.DeployedLibs;
  std::string const& LibName = DylibMsg.name();

  // send the dylib only if the client doesn't have it already
  if (DeployedLibs.count(LibName) == 0) {
    Chan.send_proto(msg::LoadDyLib, DylibMsg);
    DeployedLibs.insert(LibName);
  }
}

void ClientSession::redirect_to(SessionState &MyState, pb::ModifyFunction &MF) {
  std::string const& LibName = MF.other_lib();
  std::string const& FuncName = MF.other_name();

  // raise an error if the client doesn't already have this dylib!
  if (MyState.DeployedLibs.count(LibName) == 0)
    fatal_error("trying to redirect client to library it doesn't already have!");

  // this client is already using the right lib.
  if (MyState.CurrentLib == LibName)
    return;

  clogs() << "redirecting " << FuncName << " to " << LibName << "\n";

  auto MaybeDef = MyState.CRI.lookup(CodeRegionInfo::OriginalLib, FuncName);
  if (!MaybeDef) {
    warning("client is missing function definition for an original lib function: " + FuncName);
    return;
  }
  auto OriginalDef = MaybeDef.getValue();

  MF.set_addr(OriginalDef.Start);

  Chan.send_proto(msg::ModifyFunction, MF);
  MyState.CurrentLib = LibName;
}

void ClientSession::start(ClientGroup *CG) {
  Parent = CG;

  // We expect that the registrar has taken care of client enrollment.
  if (!Enrolled)
    clogs() << "WARNING: client is not enrolled before starting!\n";

  Parent->withClientState(this, [this](SessionState &State){
    // process this new enrollment
    State.CRI.init(Client);
    State.ID = ID;
  });

  listen();
}

void ClientSession::listen()  {
  Chan.async_recv([this](msg::Kind Kind, std::vector<char>& Body) {
    // clogs() << "got msg ID " << (uint32_t) Kind << "\n";

      switch(Kind) {
        case msg::Shutdown: {
          shutdown_async();
        } return; // NOTE: the return to ensure no more recvs are serviced.

        case msg::RawSample: {

          Parent->withClientState(this, [this,Body](SessionState &State) {
            pb::RawSample RS;
            llvm::StringRef Blob(Body.data(), Body.size());
            RS.ParseFromString(Blob.str());
            State.PerfData.add(RS);
          });

        } break;

        case msg::FunctionMeasurements: {

          Parent->withClientState(this, [this,Body](SessionState &State) {
            pb::XRayProfileData PD;
            llvm::StringRef Blob(Body.data(), Body.size());
            PD.ParseFromString(Blob.str());
            State.PerfData.add(PD);
          });

        } break;

        case msg::DyLibInfo: {

          Parent->withClientState(this, [this,Body](SessionState &State) {
            pb::DyLibInfo DLI;
            llvm::StringRef Blob(Body.data(), Body.size());
            DLI.ParseFromString(Blob.str());
            msg::print_proto(DLI);
            State.CRI.addRegion(DLI, true); // assuming DyLibInfo messages are always absolute addrs
          });

        } break;

        case msg::ClientEnroll: {
          fatal_error("recieved client enrollment when already enrolled!");
        } break;

        default: {
          logs() << "Recieved unknown message ID: "
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
