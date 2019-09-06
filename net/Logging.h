#pragma once

#include <string>
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"

namespace halo {
  // LOG tells us whether we should be noisy.
  constexpr bool LOG = false;

  inline llvm::raw_ostream& log() {
    // This function exists b/c in the future we'd like to log to a file instead.
    return LOG ? llvm::errs() : llvm::nulls();
  }

  llvm::Error makeError(const char* msg);
  llvm::Error makeError(const std::string &msg);

  inline void info(const std::string &msg) {
    if (LOG) log() << "Halomon Info: " << msg << "\n";
  }

  void warning(llvm::Error const& Error, bool MustShow = false);
  void warning(const std::string &msg, bool MustShow = false);
  void fatal_error(const std::string &msg);
}
