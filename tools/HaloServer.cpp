#include "llvm/ExecutionEngine/RTDyldMemoryManager.h"
#include "llvm/ExecutionEngine/Orc/OrcRemoteTargetServer.h"
#include "llvm/ExecutionEngine/Orc/OrcABISupport.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetSelect.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <netinet/in.h>
#include <sys/socket.h>

using namespace llvm;
using namespace llvm::orc;

// Command line argument for TCP port.

// TODO: why does this not compile?!
// cl::opt<uint32_t> Port("port",
//                        cl::desc("TCP port to listen on"),
//                        cl::init(20000));

ExitOnError ExitOnErr;

// currently based on Remote JIT example.

int main(int argc, char* argv[]) {
  if (argc == 0)
    ExitOnErr.setBanner("jit_server: ");
  else
    ExitOnErr.setBanner(std::string(argv[0]) + ": ");

  // --- Initialize LLVM ---
  cl::ParseCommandLineOptions(argc, argv, "LLVM lazy JIT example.\n");

  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();

  if (sys::DynamicLibrary::LoadLibraryPermanently(nullptr)) {
    errs() << "Error loading program symbols.\n";
    return 1;
  }


  return 0;

}
