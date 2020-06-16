#include "halo/tuner/CodeVersion.h"
#include "halo/compiler/CodeRegionInfo.h"
#include "llvm/Support/SHA1.h"


namespace halo {

  /// Creates a code version corresponding to the original library in the client.
  CodeVersion::CodeVersion() : LibName(CodeRegionInfo::OriginalLib) {
    ObjFileHashes.insert({}); // the {} here is a value initializer for std::array that is zero-filled.
  }

  /// Code version for original lib, along with its config.
  CodeVersion::CodeVersion(KnobSet OriginalConfig) : LibName(CodeRegionInfo::OriginalLib) {
    ObjFileHashes.insert({});
    Configs.push_back(std::move(OriginalConfig));
  }

  // Create a code version for a finished job.
  CodeVersion::CodeVersion(CompilationManager::FinishedJob &&Job) : LibName(Job.UniqueJobName) {
    if (!Job.Result) {
      warning("Compile job failed with an error, library is broken.");
      Broken = true;
    }

    ObjFile = std::move(Job.Result.getValue());
    SHAHash Hash = llvm::SHA1::hash(llvm::arrayRefFromStringRef(ObjFile->getBuffer()));
    ObjFileHashes.insert(Hash);
    Configs.push_back(Job.Config);
  }

  std::string const& CodeVersion::getLibraryName() const { return LibName; }

  std::unique_ptr<llvm::MemoryBuffer> const& CodeVersion::getObjectFile() const { return ObjFile; }

  /// returns true if the given code version was merged with this code version.
  /// The check is performed by comparing the object files for equality.
  /// If they're equal, this code version has its configs extended with the other's.
  bool CodeVersion::tryMerge(CodeVersion &CV) {

    bool Mergable = false;
    for (auto TheirHash : CV.ObjFileHashes)
      if (ObjFileHashes.count(TheirHash) == 1) {
        Mergable = true;
        break;
      }

    if (Mergable)
      return forceMerge(CV);

    return false;
  }

  bool CodeVersion::forceMerge(CodeVersion &CV) {

    for (auto TheirHash : CV.ObjFileHashes)
      ObjFileHashes.insert(TheirHash);

    for (auto KS : CV.Configs)
      Configs.push_back(std::move(KS));

    IPC.merge(CV.IPC);

    // empty out the other one for safety.
    CV.Configs.clear();
    CV.ObjFileHashes.clear();
    CV.IPC.clear();

    return true;
  }

  bool CodeVersion::isBroken() const { return Broken; }

  bool CodeVersion::isOriginalLib() const { return LibName == CodeRegionInfo::OriginalLib; }

  void CodeVersion::observeIPC(double value) { IPC.observe(value); }

} // end namespace
