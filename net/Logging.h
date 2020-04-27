#pragma once

#include <string>
#include <iostream>
#include <unordered_set>
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/ADT/Twine.h"

namespace halo {
  namespace __logging {
    // tells us whether we are allowed to make *any* noise at all.
  #ifdef HALO_VERBOSE
    constexpr bool NOISY = true;
  #else
    constexpr bool NOISY = false;
  #endif

    class DiscardingStream : public std::ostream {
    public:
      DiscardingStream() : std::ostream(nullptr) {}
    };

    template <class T>
    inline const DiscardingStream& operator<<(DiscardingStream &&os, const T &ignored) {
      return os;
    }


    extern std::ostream& outputStream; // where all of the logged stuff actually ends up, if enabled!
    extern DiscardingStream discardOut;

    // These have to be thread local, because the internal bookkeeping used in raw_ostream
    // classes are not completely thread-safe. Even the null stream has some bookkeeping
    // that can be invalidated when asserts are enabled.
    extern thread_local llvm::raw_os_ostream ts_raw_os_ostream;
    extern thread_local llvm::raw_null_ostream ts_null_raw_ostream;

    inline llvm::raw_ostream& nulls() {
      return ts_null_raw_ostream;
    }

  } // ena namespace __logging

  enum LoggingContext {
    LC_Anywhere,
    LC_CCT,      // calling context tree
    LC_ProgramInfoPass,
    LC_MonitorState
  };

  // master controller of what type of logging output you desire.
  inline bool loggingEnabled(LoggingContext LC) {
    switch(LC) {
      // EXPLICITLY ENABLED CONTEXTS
      case LC_Anywhere:
      // case LC_CCT:
      // case LC_ProgramInfoPass:
      // case LC_MonitorState:
        return __logging::NOISY;

      default:
        break;
    };
    return false; // DISABLED
  }

  // the output logging stream (LLVM raw ostream), which is not thread-safe!
  inline llvm::raw_ostream& logs(LoggingContext LC = LC_Anywhere) {
    return loggingEnabled(LC) ? __logging::ts_raw_os_ostream : __logging::nulls();
  }

  // an alternative logging stream based on std::ostream
  inline std::ostream& clogs(LoggingContext LC = LC_Anywhere) {
    return loggingEnabled(LC) ? __logging::outputStream : __logging::discardOut;
  }

  llvm::Error makeError(const char* msg);
  llvm::Error makeError(const std::string &msg);
  llvm::Error makeError(llvm::Twine msg);

  void info(const std::string &msg);
  void info(const char *msg);

  void warning(llvm::Error const& Error);
  void warning(const std::string &msg);

  void fatal_error(const std::string &msg);
}
