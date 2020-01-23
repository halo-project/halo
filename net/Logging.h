#pragma once

#include <string>
#include <iostream>
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Twine.h"

namespace halo {
  // LOG tells us whether we should be noisy.
#ifdef HALO_VERBOSE
  constexpr bool LOG = true;
#else
  constexpr bool LOG = false;
#endif

  // the output logging stream (LLVM raw ostream)
  inline llvm::raw_ostream& logs() {
    // This function exists b/c in the future we'd like to log to a file instead.
    return LOG ? llvm::errs() : llvm::nulls();
  }

  // an alternative logging stream based on std::ostream
  inline std::ostream& clogs() {
    // FIXME: offer a /dev/null or dummy ostream when not logging!
    return std::cerr;
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
