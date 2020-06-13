#pragma once

#include <vector>
#include <array>
#include <memory>

#include "halo/tuner/RandomQuantity.h"
#include "halo/tuner/KnobSet.h"
#include "halo/server/CompilationManager.h"

#include "llvm/Support/MemoryBuffer.h"


namespace halo {

/// A compiled instance of a configuration with respect to the bitcode.
/// colloquially a "dylib" or library.
class CodeVersion {
  public:
  /// Creates a code version corresponding to the original library in the client.
  CodeVersion();

  /// Code version for original lib, along with its config.
  CodeVersion(KnobSet OriginalConfig);

  // Create a code version for a finished job.
  CodeVersion(CompilationManager::FinishedJob &&Job);

  std::string const& getLibraryName() const;

  std::unique_ptr<llvm::MemoryBuffer> const& getObjectFile() const;

  /// returns true if the given code version was merged with this code version.
  /// The check is performed by comparing the object files for equality.
  /// If they're equal, this code version has its configs extended with the other's.
  bool tryMerge(CodeVersion &CV);

  bool isBroken() const;

  bool isOriginalLib() const;

  void observeIPC(double value);

  RandomQuantity& getIPC();

  private:
  bool Broken{false};
  std::string LibName;
  std::unique_ptr<llvm::MemoryBuffer> ObjFile;
  std::array<uint8_t, 20> ObjFileHash;
  std::vector<KnobSet> Configs;
  RandomQuantity IPC{50};
};

} // end namespace