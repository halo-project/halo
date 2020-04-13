#pragma once

#include <string>
#include <iostream>
#include <unordered_set>
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"
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
    inline const DiscardingStream& operator << (DiscardingStream &&os, const T &ignored) {
      return os;
    }

    extern DiscardingStream discardOut;

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

  // the output logging stream (LLVM raw ostream)
  inline llvm::raw_ostream& logs(LoggingContext LC = LC_Anywhere) {
    // This function exists b/c in the future we'd like to log to a file instead.
    return loggingEnabled(LC) ? llvm::errs() : llvm::nulls();
  }

  // an alternative logging stream based on std::ostream
  inline std::ostream& clogs(LoggingContext LC = LC_Anywhere) {
    // FIXME: offer a /dev/null or dummy ostream when not logging!
    return loggingEnabled(LC) ? std::cerr : __logging::discardOut;
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
