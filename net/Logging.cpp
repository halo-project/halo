
#include "Logging.h"
#include <cstdlib>
#include <unistd.h>

namespace boost {

void throw_exception(std::exception const& ex) {
  halo::clogs(halo::LC_Error) << "uncaught exception: " << ex.what() << "\n";
  std::exit(EXIT_FAILURE);
}

}

namespace halo {
  llvm::Error makeError(const char* msg) {
    return llvm::createStringError(std::errc::operation_not_supported, msg);
  }

  llvm::Error makeError(const std::string &msg) {
    return makeError(msg.c_str());
  }

  llvm::Error makeError(llvm::Twine msg) {
    return makeError(msg.str());
  }

  const char* WarnTag = "halo warning: ";
  const char* InfoTag = "halo info: ";
  const char* ServerInfoTag = "haloserver: ";
  const char* ErrorTag = "halo fatal error: ";

  [[ noreturn ]] void fatal_error(const std::string &msg) {
    clogs(LC_Error) << ErrorTag << msg << "\n";
    std::exit(EXIT_FAILURE);
  }

  [[ noreturn ]] void fatal_error(llvm::Error &&Error) {
    logs(LC_Error) << ErrorTag << Error << "\n";
    std::exit(EXIT_FAILURE);
  }

  void warning(llvm::Error const& err) {
    logs(LC_Warning) << WarnTag << err << "\n";
  }

  void warning(const std::string &msg) {
    clogs(LC_Warning) << WarnTag << msg << "\n";
  }

  void info(llvm::Error const& err) {
    logs(LC_Info) << InfoTag << err << "\n";
  }

  void info(const char *msg) {
    clogs(LC_Info) << InfoTag << msg << "\n";
  }

  void info(const std::string &msg) {
    clogs(LC_Info) << InfoTag << msg << "\n";
  }

  void server_info(const char *msg) {
    clogs(LC_Server) << ServerInfoTag << msg << "\n";
  }

  void server_info(const std::string &msg) {
    clogs(LC_Server) << ServerInfoTag << msg << "\n";
  }

  namespace __logging {
    // NOTE: if you ever want to log to a single file, make sure you
    // sync up these outputs to go to the same file descriptor!
    std::ostream& outputStream(std::cerr);
    thread_local llvm::raw_fd_ostream ts_output_raw_ostream(STDERR_FILENO,
                                            /*shouldClose*/ false, /*unbuffered*/ true);

    DiscardingStream discardOut;
    thread_local llvm::raw_null_ostream ts_null_raw_ostream;
  }
}
