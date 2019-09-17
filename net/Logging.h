#pragma once

#include <string>
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

  inline llvm::raw_ostream& log() {
    // This function exists b/c in the future we'd like to log to a file instead.
    // return LOG ? llvm::errs() : llvm::nulls();
    return llvm::errs();
  }

  llvm::Error makeError(const char* msg);
  llvm::Error makeError(const std::string &msg);
  llvm::Error makeError(llvm::Twine msg);

  void info(const std::string &msg, bool MustShow = false);
  void info(const char *msg, bool MustShow = false);

  void warning(llvm::Error const& Error, bool MustShow = false);
  void warning(const std::string &msg, bool MustShow = false);

  void fatal_error(const std::string &msg);
}
