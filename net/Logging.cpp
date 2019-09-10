
#include "Logging.h"
#include <cstdlib>

namespace boost {

void throw_exception(std::exception const& ex) {
  if (halo::LOG) halo::log() << "uncaught exception: " << ex.what() << "\n";
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

  void fatal_error(const std::string &msg) {
    llvm::report_fatal_error(makeError(msg));
  }

  const char* WarnTag = "halo warning: ";
  const char* InfoTag = "halo info: ";

  void warning(llvm::Error const& err, bool MustShow) {
    if (LOG || MustShow) log() << WarnTag << err << "\n";
  }

  void warning(const std::string &msg, bool MustShow) {
    if (LOG || MustShow) log() << WarnTag << msg << "\n";
  }

  void info(const char *msg, bool MustShow) {
    if (LOG || MustShow) log() << InfoTag << msg << "\n";
  }
}
