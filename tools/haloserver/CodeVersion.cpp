#include "halo/tuner/CodeVersion.h"
#include "halo/compiler/CodeRegionInfo.h"
#include "llvm/Support/SHA1.h"


namespace halo {

  /// Creates a code version corresponding to the original library in the client.
  CodeVersion::CodeVersion() : LibName(CodeRegionInfo::OriginalLib) {
    std::fill(ObjFileHash.begin(), ObjFileHash.end(), 0);
  }

  /// Code version for original lib, along with its config.
  CodeVersion::CodeVersion(KnobSet OriginalConfig) : LibName(CodeRegionInfo::OriginalLib) {
    std::fill(ObjFileHash.begin(), ObjFileHash.end(), 0);
    Configs.push_back(std::move(OriginalConfig));
  }

  // Create a code version for a finished job.
  CodeVersion::CodeVersion(CompilationManager::FinishedJob &&Job) : LibName(Job.UniqueJobName) {
    if (!Job.Result) {
      warning("Compile job failed with an error, library is broken.");
      Broken = true;
    }

    ObjFile = std::move(Job.Result.getValue());
    ObjFileHash = llvm::SHA1::hash(llvm::arrayRefFromStringRef(ObjFile->getBuffer()));
    Configs.push_back(Job.Config);
  }

  std::string const& CodeVersion::getLibraryName() const { return LibName; }

  std::unique_ptr<llvm::MemoryBuffer> const& CodeVersion::getObjectFile() const { return ObjFile; }

  /// returns true if the given code version was merged with this code version.
  /// The check is performed by comparing the object files for equality.
  /// If they're equal, this code version has its configs extended with the other's.
  bool CodeVersion::tryMerge(CodeVersion &CV) {
    if (ObjFileHash != CV.ObjFileHash)
      return false;

    for (auto KS : CV.Configs)
      Configs.push_back(std::move(KS));

    // TODO: should we merge other stuff, like IPC?
    // currently I only see calling this on a fresh CV.
    assert(CV.IPC.observations() == 0 && "see TODO above");

    CV.Configs.clear();
    return true;
  }

  bool CodeVersion::isBroken() const { return Broken; }

  bool CodeVersion::isOriginalLib() const { return LibName == CodeRegionInfo::OriginalLib; }

  void CodeVersion::observeIPC(double value) { IPC.observe(value); }

  size_t CodeVersion::recordedIPCs() const { return IPC.observations(); }

  // returns true if this code version is better than the given one, and false otherwise.
  // if the query cannot be answered, then NONE is returned instead.
  llvm::Optional<bool> CodeVersion::betterThan(CodeVersion const& Other) const {
    auto Me = IPC.mean();
    auto Them = Other.IPC.mean();

    return Me >= Them;
  }





} // end namespace
